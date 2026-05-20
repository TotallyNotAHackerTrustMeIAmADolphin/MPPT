/**
  ******************************************************************************
  * @file           : controller.c
  * @brief          : Unified Override Control (Min-Selector) implementation.
  ******************************************************************************
  */

#include "controller.h"
#include "system_config.h"
#include "sensors.h"
#include "power.h"
#include "mppt.h"
#include "settings.h"
#include "comms.h"
#include "stm32f0xx_hal.h"
#include <stdio.h>

/* Private variables (Encapsulation) */
static SystemState_t currentState = STATE_IDLE;
static FaultReason_t currentFaultReason = FAULT_REASON_NONE;
static uint32_t lastMPPTTick = 0;
static uint32_t lastSweepTick = 0;
static uint32_t lastTelemetryTick = 0;

/* High-resolution duty cycle accumulator (Velocity PI integral) */
static int64_t globalDutyIntegral = 0;
static int32_t targetDuty_ticks = 0;

/* Control Gains (Scaled for 1ms task) */
#define GAIN_KP 10
#define GAIN_KI 2

/* Velocity PI storage for derivative terms */
static int32_t lastVout = 0;
static int32_t lastIout = 0;

static uint8_t faultCounter = 0;
#define FAULT_THRESHOLD_FRAMES 3

/* Internal State Transition Logic */
static void transitionTo(SystemState_t newState) {
    if (currentState == newState) return;
    
    // State Exit Actions
    if (currentState == STATE_FAULT || currentState == STATE_IDLE) {
        if (newState != STATE_FAULT && newState != STATE_IDLE) {
            POWER_Start();
        }
    }

    // State Entry Actions
    switch (newState) {
        case STATE_SWEEPING:
            MPPT_ResetSweep();
            break;
        case STATE_ACTIVE:
            globalDutyIntegral = (int64_t)targetDuty_ticks * 1000;
            lastVout = SENSORS_GetMeasurements()->voltageOut_mV;
            lastIout = SENSORS_GetMeasurements()->currentOut_mA;
            MPPT_StartTracking(SENSORS_GetMeasurements());
            break;
        case STATE_IDLE:
            POWER_Shutdown();
            currentFaultReason = FAULT_REASON_NONE;
            faultCounter = 0;
            break;
        case STATE_FAULT:
            POWER_Shutdown();
            break;
        default:
            break;
    }
    
    printf("STATE: %s -> %s\n", CONTROLLER_GetStateString(), 
           (newState == STATE_IDLE) ? "IDLE" :
           (newState == STATE_SWEEPING) ? "SWEEPING" :
           (newState == STATE_ACTIVE) ? "ACTIVE" :
           (newState == STATE_FAULT) ? "FAULT" : "RECOVERY");
           
    currentState = newState;
}

void CONTROLLER_Init(void) {
    currentState = STATE_IDLE;
    currentFaultReason = FAULT_REASON_NONE;
    lastMPPTTick = 0;
    lastSweepTick = 0;
    lastTelemetryTick = 0;
    globalDutyIntegral = 0;
    targetDuty_ticks = 0;
    lastVout = 0;
    lastIout = 0;
}

void CONTROLLER_UpdateHighRate(void) {
    const Measurements_t *m = SENSORS_GetMeasurements();
    const DeviceLimits_t *limits = SETTINGS_GetLimits();
    uint32_t currentTick = HAL_GetTick();

    // 1. Safety Logic (Hard Hardware Limits)
    FaultReason_t newFault = FAULT_REASON_NONE;

    if (m->voltageIn_mV > HARD_LIMIT_VIN_MAX_MV) {
        newFault = FAULT_REASON_INPUT_OV;
    } else if (currentState != STATE_IDLE && m->voltageIn_mV < HARD_LIMIT_VIN_MIN_MV) {
        newFault = FAULT_REASON_INPUT_UV;
    } else if (m->currentIn_mA > HARD_LIMIT_IIN_MAX_MA) {
        newFault = FAULT_REASON_INPUT_OC;
    } else if (m->voltageOut_mV > HARD_LIMIT_VOUT_MAX_MV) {
        newFault = FAULT_REASON_OUTPUT_OV;
    } else if (m->currentOut_mA > HARD_LIMIT_IOUT_MAX_MA) {
        newFault = FAULT_REASON_OUTPUT_OC;
    } else if (m->tempMCU_C_x100 > (HARD_LIMIT_TEMP_MAX_C * 100)) {
        newFault = FAULT_REASON_OVERTEMP;
    } else if (limits->mode != MODE_EBIKE && m->currentOut_mA < -500) {
        newFault = FAULT_REASON_BACKFLOW;
    }

    if (newFault != FAULT_REASON_NONE) {
        faultCounter++;
        if (faultCounter >= FAULT_THRESHOLD_FRAMES) {
            currentFaultReason = newFault;
            transitionTo(STATE_FAULT);
        }
    } else {
        faultCounter = 0;
    }

    // 2. Control Logic
    switch (currentState) {
        case STATE_FAULT:
            targetDuty_ticks = 0;
            if (currentFaultReason == FAULT_REASON_INPUT_UV && m->voltageIn_mV > MIN_VOLTAGE_IN_MV) {
                transitionTo(STATE_IDLE);
            }
            break;

        case STATE_IDLE:
            targetDuty_ticks = 0;
            if (m->voltageIn_mV > MIN_VOLTAGE_IN_MV) {
                if (limits->mode == MODE_MPPT) transitionTo(STATE_SWEEPING);
                else transitionTo(STATE_ACTIVE);
            }
            break;

        case STATE_ACTIVE: {
            // --- Multi-Variable Override Control (Min-Selector) ---
            int64_t min_delta = 1000000; // Large positive default (1000 ticks)

            // A. Forward Voltage Limit (CV)
            // Velocity PI: delta = Ki*error - Kp*dInput
            int64_t error_V = (int64_t)limits->outputVoltageMax_mV - m->voltageOut_mV;
            int64_t dInput_V = (int64_t)m->voltageOut_mV - lastVout;
            int64_t delta_Vout = (GAIN_KI * error_V) - (GAIN_KP * dInput_V);
            if (delta_Vout < min_delta) min_delta = delta_Vout;

            // B. Forward Current Limit (CC)
            int64_t error_I = (int64_t)limits->outputCurrentMax_mA - m->currentOut_mA;
            int64_t dInput_I = (int64_t)m->currentOut_mA - lastIout;
            int64_t delta_Iout = (GAIN_KI * error_I) - (GAIN_KP * dInput_I);
            if (delta_Iout < min_delta) min_delta = delta_Iout;

            // C. Input Brownout Protection (Hard floor)
            if (m->voltageIn_mV < limits->inputVoltageMin_mV) {
                int64_t delta_brownout = -19000; // Hard backoff (ticks*1000)
                if (delta_brownout < min_delta) min_delta = delta_brownout;
            }

            // D. E-Bike Specific Regen Limits
            if (limits->mode == MODE_EBIKE) {
                if (m->currentOut_mA < 0) {
                    // Regen Voltage (Protect battery from overcharge)
                    int64_t delta_RegenV = (int64_t)GAIN_KI * (limits->inputVoltageMax_mV - m->voltageIn_mV);
                    if (delta_RegenV < min_delta) min_delta = delta_RegenV;

                    // Regen Current (Negative Iout out)
                    int64_t delta_RegenI = (int64_t)GAIN_KI * (m->currentOut_mA + limits->inputCurrentMax_mA);
                    if (delta_RegenI < min_delta) min_delta = delta_RegenI;
                }

                // E. Soft Disconnect: Prevent forward drive if battery is low
                if (m->voltageIn_mV < limits->inputVoltageMin_mV && min_delta > 0) {
                    min_delta = 0;
                }
            } else if (limits->mode == MODE_POWER_SUPPLY) {
                // PSU Mode: Block logical duty increase if reverse current detected
                if (m->currentOut_mA < -100 && min_delta > 0) min_delta = 0;
            }

            // F. MPPT Algorithm Participation
            // (Only runs at MPPT_INTERVAL_MS, otherwise delta is effectively infinite)
            if (limits->mode == MODE_MPPT && (currentTick - lastMPPTTick >= MPPT_GetInterval())) {
                lastMPPTTick = currentTick;
                int32_t mppt_delta_ticks = MPPT_PerturbAndObserve(m, limits);
                int64_t delta_MPPT = (int64_t)mppt_delta_ticks * 1000;
                if (delta_MPPT < min_delta) min_delta = delta_MPPT;
            }

            // 3. Accumulate and Apply
            globalDutyIntegral += min_delta;
            
            // Constrain logical state
            int64_t maxIntegral = (int64_t)POWER_PWM_GetMax() * 1000;
            if (globalDutyIntegral < 0) globalDutyIntegral = 0;
            if (globalDutyIntegral > maxIntegral) globalDutyIntegral = maxIntegral;

            targetDuty_ticks = (int32_t)(globalDutyIntegral / 1000);
            
            // Update derivative terms
            lastVout = m->voltageOut_mV;
            lastIout = m->currentOut_mA;

            // Return to IDLE if input voltage is lost
            if (m->voltageIn_mV < (MIN_VOLTAGE_IN_MV - 1000)) transitionTo(STATE_IDLE);
            break;
        }

        default:
            break;
    }

    // Override for calibration
    if (SETTINGS_IsCalibrating()) {
        targetDuty_ticks = SETTINGS_IsCalHighSideOn() ? (TIMER_PERIOD * DITHER_TABLE_SIZE) : 0;
    }

    POWER_PWM_Set(targetDuty_ticks);
}

void CONTROLLER_Task(void) {
    uint32_t currentTick = HAL_GetTick();
    const Measurements_t *m = SENSORS_GetMeasurements();
    const DeviceLimits_t *limits = SETTINGS_GetLimits();

    // 1. Telemetry (10Hz)
    if (currentTick - lastTelemetryTick >= TELEMETRY_INTERVAL_MS) {
        lastTelemetryTick = currentTick;
        COMMS_SendTelemetry(m);
    }

    // 2. Sweeping Logic (Managed here as it's a global search)
    if (currentState == STATE_SWEEPING) {
        bool finished = false;
        targetDuty_ticks = MPPT_RunSweep(m, limits, &finished);
        if (finished) {
            transitionTo(STATE_ACTIVE);
            lastSweepTick = currentTick;
        }
    }

    // 3. Periodic Sweep (Solar mode only)
    if (currentState == STATE_ACTIVE && limits->mode == MODE_MPPT) {
        if (currentTick - lastSweepTick >= (uint32_t)SWEEP_INTERVAL_SECONDS * 1000) {
            transitionTo(STATE_SWEEPING);
        }
    }
}

SystemState_t CONTROLLER_GetState(void) {
    return currentState;
}

const char* CONTROLLER_GetStateString(void) {
    switch (currentState) {
        case STATE_IDLE:     return "IDLE";
        case STATE_SWEEPING: return "SWEEPING";
        case STATE_ACTIVE:   return "ACTIVE";
        case STATE_FAULT:    return "FAULT";
        case STATE_RECOVERY: return "RECOVERY";
        default:             return "UNKNOWN";
    }
}

const char* CONTROLLER_GetFaultReasonString(void) {
    switch (currentFaultReason) {
        case FAULT_REASON_NONE:      return "NONE";
        case FAULT_REASON_INPUT_OV:  return "INPUT_OVERVOLTAGE";
        case FAULT_REASON_INPUT_UV:  return "INPUT_UNDERVOLTAGE";
        case FAULT_REASON_INPUT_OC:  return "INPUT_OVERCURRENT";
        case FAULT_REASON_OUTPUT_OV: return "OUTPUT_OVERVOLTAGE";
        case FAULT_REASON_OUTPUT_OC: return "OUTPUT_OVERCURRENT";
        case FAULT_REASON_BACKFLOW:  return "BACKFLOW";
        case FAULT_REASON_OVERTEMP:  return "OVERTEMPERATURE";
        default:                     return "UNKNOWN";
    }
}

void CONTROLLER_ResetFault(void) {
    if (currentState == STATE_FAULT) {
        transitionTo(STATE_IDLE);
    }
}

void CONTROLLER_Reset(void) {
    targetDuty_ticks = 0;
    globalDutyIntegral = 0;
    POWER_PWM_Set(0);
    transitionTo(STATE_IDLE);
}
