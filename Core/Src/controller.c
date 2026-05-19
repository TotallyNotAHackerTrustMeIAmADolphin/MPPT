/**
  ******************************************************************************
  * @file           : controller.c
  * @brief          : Unified System State Machine implementation.
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

static PID_t pidCV;
static PID_t pidCC;
static int32_t targetDuty_ticks = 0;
static uint8_t faultCounter = 0;
#define FAULT_THRESHOLD_FRAMES 3

/* Internal State Transition Logic */
static void transitionTo(SystemState_t newState) {
    if (currentState == newState) return;
    
    // State Entry Actions
    switch (newState) {
        case STATE_SWEEPING:
            MPPT_ResetSweep();
            break;
        case STATE_MPPT:
            MPPT_StartTracking(SENSORS_GetMeasurements());
            break;
        case STATE_CV:
            pidCV.integral = (int64_t)targetDuty_ticks * 1000;
            pidCV.previousError = 0;
            pidCV.previousInput = *pidCV.input;
            break;
        case STATE_CC:
            pidCC.integral = (int64_t)targetDuty_ticks * 1000;
            pidCC.previousError = 0;
            pidCC.previousInput = *pidCC.input;
            break;
        case STATE_IDLE:
            currentFaultReason = FAULT_REASON_NONE;
            faultCounter = 0;
            break;
        default:
            break;
    }
    
    printf("STATE: %s -> %s\n", CONTROLLER_GetStateString(), 
           (newState == STATE_IDLE) ? "IDLE" :
           (newState == STATE_SWEEPING) ? "SWEEPING" :
           (newState == STATE_MPPT) ? "MPPT" :
           (newState == STATE_CV) ? "CV" :
           (newState == STATE_CC) ? "CC" :
           (newState == STATE_FAULT) ? "FAULT" : "RECOVERY");
           
    currentState = newState;
}

void CONTROLLER_Init(void) {
    currentState = STATE_IDLE;
    currentFaultReason = FAULT_REASON_NONE;
    lastMPPTTick = 0;
    lastSweepTick = 0;
    lastTelemetryTick = 0;

    const DeviceLimits_t *limits = SETTINGS_GetLimits();
    const Measurements_t *m = SENSORS_GetMeasurements();

    // Initialize PID controllers (tuned for 200Hz high-rate task using Velocity PI)
    pidCV.Kp = 10; pidCV.Ki = 2; pidCV.Kd = 0;
    pidCV.previousError = 0; 
    pidCV.previousInput = m->voltageOut_mV;
    pidCV.integral = 0;
    pidCV.setPoint = limits->batteryMax_mV;
    pidCV.input = (int32_t*)&m->voltageOut_mV;
    pidCV.output = &targetDuty_ticks;
    pidCV.maxIntegral = 20000;
    pidCV.minOutput = 0;
    pidCV.maxOutput = POWER_PWM_GetMax();

    pidCC.Kp = 10; pidCC.Ki = 2; pidCC.Kd = 0;
    pidCC.previousError = 0;
    pidCC.previousInput = m->currentOut_mA;
    pidCC.integral = 0;
    pidCC.setPoint = limits->chargingCurrent_mA;
    pidCC.input = (int32_t*)&m->currentOut_mA;
    pidCC.output = &targetDuty_ticks;
    pidCC.maxIntegral = 20000;
    pidCC.minOutput = 0;
    pidCC.maxOutput = POWER_PWM_GetMax();
}

void CONTROLLER_UpdateHighRate(void) {
    const Measurements_t *m = SENSORS_GetMeasurements();
    const DeviceLimits_t *limits = SETTINGS_GetLimits();
    uint32_t currentTick = HAL_GetTick();

    // 1. Safety Logic (Highest Priority - Hard Hardware Limits)
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
    } else if (m->currentOut_mA < -500) {
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

    // 2. State-Specific High-Rate Logic
    switch (currentState) {
        case STATE_FAULT:
            targetDuty_ticks = 0;
            // Auto-recovery from brownout (Input Under-voltage)
            if (currentFaultReason == FAULT_REASON_INPUT_UV && m->voltageIn_mV > MIN_VOLTAGE_IN_MV) {
                transitionTo(STATE_IDLE);
            }
            break;

        case STATE_IDLE:
            targetDuty_ticks = 0;
            if (m->voltageIn_mV > MIN_VOLTAGE_IN_MV) {
                transitionTo(STATE_SWEEPING);
            }
            break;

        case STATE_CV:
            pidCV.setPoint = limits->batteryMax_mV; // Keep updated from limits
            POWER_PID_Compute(&pidCV);
            
            // Input Voltage Sag Protection (Brownout prevention)
            if (m->voltageIn_mV < MIN_INPUT_VOLTAGE_MPPT_MV) {
                targetDuty_ticks -= 19; // Back off hard if source sags
                if (targetDuty_ticks < 0) targetDuty_ticks = 0;
            }

            // Only exit CV if voltage drops significantly below setpoint
            if (m->voltageOut_mV < (limits->batteryMax_mV - HYSTERESIS_VOLTAGE_MV)) {
                transitionTo(STATE_MPPT);
            }
            break;

        case STATE_CC:
            pidCC.setPoint = limits->chargingCurrent_mA; // Keep updated
            POWER_PID_Compute(&pidCC);

            // Input Voltage Sag Protection (Brownout prevention)
            if (m->voltageIn_mV < MIN_INPUT_VOLTAGE_MPPT_MV) {
                targetDuty_ticks -= 19;
                if (targetDuty_ticks < 0) targetDuty_ticks = 0;
            }

            // Only exit CC if current drops significantly below limit
            if (m->currentOut_mA < (limits->chargingCurrent_mA - HYSTERESIS_CURRENT_MA)) {
                transitionTo(STATE_MPPT);
            }
            break;

        default:
            // MPPT or Sweeping states use timed-rate updates for duty cycle,
            // but we check for CV/CC transitions here at high rate.
            if (m->voltageOut_mV >= limits->batteryMax_mV) {
                transitionTo(STATE_CV);
                lastSweepTick = currentTick; // Postpone sweep if we hit limit
            } else if (m->currentOut_mA >= limits->chargingCurrent_mA) {
                transitionTo(STATE_CC);
                lastSweepTick = currentTick;
            }
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

    // 2. Control Logic (Timed)
    if (currentTick - lastMPPTTick >= MPPT_GetInterval()) {
        lastMPPTTick = currentTick;

        switch (currentState) {
            case STATE_SWEEPING: {
                bool finished = false;
                targetDuty_ticks = MPPT_RunSweep(m, &finished);
                if (finished) {
                    transitionTo(STATE_MPPT);
                    lastSweepTick = currentTick;
                }
                break;
            }

            case STATE_MPPT:
                // Check if it's time for a periodic sweep
                if (currentTick - lastSweepTick >= (uint32_t)SWEEP_INTERVAL_SECONDS * 1000) {
                    transitionTo(STATE_SWEEPING);
                } else {
                    targetDuty_ticks = MPPT_PerturbAndObserve(m, limits);
                }
                break;

            case STATE_FAULT:
                // Recovery logic?
                break;

            default:
                break;
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
        case STATE_MPPT:     return "MPPT";
        case STATE_CV:       return "CV";
        case STATE_CC:       return "CC";
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
