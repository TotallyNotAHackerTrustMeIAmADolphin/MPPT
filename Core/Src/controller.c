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
#include "main.h"
#ifndef NATIVE_TEST
#include "stm32f0xx_hal.h"
#endif
#include <stdio.h>

/* Private variables (Encapsulation) */
static SystemState_t currentState = STATE_IDLE;
static FaultReason_t currentFaultReason = FAULT_REASON_NONE;
static SoftLimit_t activeSoftLimit = LIMIT_NONE;
static uint32_t lastMPPTTick = 0;
static uint32_t lastSweepTick = 0;
static uint32_t lastSweepStepTick = 0;
static uint32_t lastTelemetryTick = 0;

/* High-resolution duty cycle accumulator (Velocity PI integral) */
static int64_t globalDutyIntegral = 0;
static int32_t targetDuty_ticks = 0;

/* Control Gains (Scaled for 1ms task) */
#define GAIN_KP 5
#define GAIN_KI 2

/* Velocity PI storage for derivative terms */
static int32_t lastVout = 0;
static int32_t lastIout = 0;

static uint8_t faultCounter = 0;
#define FAULT_THRESHOLD_FRAMES 3

static uint32_t softLimitHoldTimer = 0;
#define SOFT_LIMIT_HOLD_TIME_MS 100
#define SWEEP_STEP_INTERVAL_MS 20
#define FAULT_RECOVERY_DELAY_MS 5000

static uint32_t faultStateEntryTick = 0;

/* Fault Blink Logic */
static uint32_t lastFaultBlinkTick = 0;
static uint8_t blinkPhase = 0;
static uint8_t blinkCount = 0;

static void handleFaultBlink(FaultReason_t reason) {
    uint32_t currentTick = HAL_GetTick();
    uint8_t targetBlinks = 0;
    
    // Define blink codes based on fault reason
    switch (reason) {
        case FAULT_REASON_INPUT_OV:  targetBlinks = 1; break;
        case FAULT_REASON_INPUT_UV:  targetBlinks = 2; break;
        case FAULT_REASON_INPUT_OC:  targetBlinks = 3; break;
        case FAULT_REASON_OUTPUT_OV: targetBlinks = 4; break;
        case FAULT_REASON_OUTPUT_OC: targetBlinks = 5; break;
        case FAULT_REASON_BACKFLOW:  targetBlinks = 6; break;
        case FAULT_REASON_OVERTEMP:  targetBlinks = 7; break;
        default:                     targetBlinks = 0; break;
    }

    if (targetBlinks == 0) {
        HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
        return;
    }

    // Blink timing: 200ms ON, 200ms OFF, 1000ms pause between cycles
    uint32_t elapsed = currentTick - lastFaultBlinkTick;

    if (blinkPhase == 0) { // Active blinking phase
        if (elapsed >= 200) {
            lastFaultBlinkTick = currentTick;
            HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
            
            // If we just toggled to OFF (GPIO_PIN_RESET)
            if (HAL_GPIO_ReadPin(LED_GPIO_Port, LED_Pin) == GPIO_PIN_RESET) {
                blinkCount++;
                if (blinkCount >= targetBlinks) {
                    blinkPhase = 1; // Start pause phase
                    blinkCount = 0;
                }
            }
        }
    } else { // Pause phase
        if (elapsed >= 1000) {
            lastFaultBlinkTick = currentTick;
            blinkPhase = 0;
        }
    }
}

/* Internal State Transition Logic */
static void transitionTo(SystemState_t newState) {
    if (currentState == newState) return;
    
    // State Exit Actions
    if (currentState == STATE_FAULT || currentState == STATE_IDLE) {
        if (newState != STATE_FAULT && newState != STATE_IDLE) {
            const Measurements_t *m = SENSORS_GetMeasurements();
            targetDuty_ticks = POWER_CalculateVoltageMatchDuty(m->voltageIn_mV, m->voltageOut_mV);
            POWER_Start();
        }
    }

    // State Entry Actions
    switch (newState) {
        case STATE_SWEEPING:
            MPPT_ResetSweep(targetDuty_ticks);
            activeSoftLimit = LIMIT_SWEEPING;
            lastSweepStepTick = HAL_GetTick();
            break;
        case STATE_ACTIVE:
            globalDutyIntegral = (int64_t)targetDuty_ticks * 1000;
            lastVout = SENSORS_GetMeasurements()->voltageOut_mV;
            lastIout = SENSORS_GetMeasurements()->currentOut_mA;
            MPPT_StartTracking(SENSORS_GetMeasurements());
            activeSoftLimit = LIMIT_NONE;
            break;
        case STATE_IDLE:
            POWER_Shutdown();
            currentFaultReason = FAULT_REASON_NONE;
            faultCounter = 0;
            activeSoftLimit = LIMIT_NONE;
            HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
            break;
        case STATE_FAULT:
            POWER_Shutdown();
            activeSoftLimit = LIMIT_NONE;
            lastFaultBlinkTick = HAL_GetTick();
            faultStateEntryTick = HAL_GetTick();
            blinkPhase = 0;
            blinkCount = 0;
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
    activeSoftLimit = LIMIT_NONE;
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
    } else if (limits->mode == MODE_MPPT && m->currentOut_mA < -1000) {
        newFault = FAULT_REASON_BACKFLOW;
    }

    if (newFault != FAULT_REASON_NONE && !SETTINGS_IsCalibrating()) {
        faultCounter++;
        if (faultCounter >= FAULT_THRESHOLD_FRAMES) {
            currentFaultReason = newFault;
            transitionTo(STATE_FAULT);
        }
    } else {
        faultCounter = 0;
    }

    // 2. Control Logic
    if (softLimitHoldTimer > 0) softLimitHoldTimer--;
    
    switch (currentState) {
        case STATE_FAULT:
            targetDuty_ticks = 0;
            // Auto-recovery: If input UV is gone OR if it was a transient fault (like backflow), wait then retry
            if (currentFaultReason == FAULT_REASON_INPUT_UV) {
                if (m->voltageIn_mV > MIN_VOLTAGE_IN_MV) transitionTo(STATE_IDLE);
            } else if (currentTick - faultStateEntryTick >= FAULT_RECOVERY_DELAY_MS) {
                // Try to recover from other faults after a delay
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
            
            // Baseline: What do we WANT to do if no limits are hit?
            int64_t min_delta = 1000000; // Default to "increase" in CC/CV modes
            
            // Only update soft limit if the hold timer has expired
            if (softLimitHoldTimer == 0) {
                activeSoftLimit = LIMIT_NONE;
            }

            if (limits->mode == MODE_MPPT) {
                if (currentTick - lastMPPTTick >= MPPT_GetInterval()) {
                    lastMPPTTick = currentTick;
#if ACTIVE_MPPT_ALGO == MPPT_ALGO_INC_COND
                    int32_t mppt_delta_ticks = MPPT_IncrementalConductance(m, limits);
#else
                    int32_t mppt_delta_ticks = MPPT_PerturbAndObserve(m, limits);
#endif
                    min_delta = (int64_t)mppt_delta_ticks * 1000;
                } else {
                    min_delta = 0; // Hold duty between MPPT steps
                }
            }

            // A. Output Voltage Limit (Forward CV)
            int64_t error_Vout = (int64_t)limits->vOutMax_mV - m->voltageOut_mV;
            int64_t dInput_Vout = (int64_t)m->voltageOut_mV - lastVout;
            int64_t delta_Vout = (GAIN_KI * error_Vout) - (GAIN_KP * dInput_Vout);
            if (delta_Vout < min_delta) {
                min_delta = delta_Vout;
                if (limits->mode != MODE_MPPT || delta_Vout <= 0) {
                    activeSoftLimit = LIMIT_V_OUT_MAX;
                    softLimitHoldTimer = SOFT_LIMIT_HOLD_TIME_MS;
                }
            }

            // B. Output Current Max Limit (Forward CC)
            int64_t error_IoutMax = (int64_t)limits->iOutMax_mA - m->currentOut_mA;
            int64_t dInput_Iout = (int64_t)m->currentOut_mA - lastIout;
            int64_t delta_IoutMax = (GAIN_KI * error_IoutMax) - (GAIN_KP * dInput_Iout);
            if (delta_IoutMax < min_delta) {
                min_delta = delta_IoutMax;
                if (limits->mode != MODE_MPPT || delta_IoutMax <= 0) {
                    activeSoftLimit = LIMIT_I_OUT_MAX;
                    softLimitHoldTimer = SOFT_LIMIT_HOLD_TIME_MS;
                }
            }

            // C. Output Current Min Limit (Reverse CC / Backflow)
            int64_t error_IoutMin = (int64_t)m->currentOut_mA - limits->iOutMin_mA;
            if (error_IoutMin < 0) {
                int64_t delta_IoutMin = (int64_t)GAIN_KI * error_IoutMin * 5; 
                if (delta_IoutMin < min_delta) {
                    min_delta = delta_IoutMin;
                    activeSoftLimit = LIMIT_I_OUT_MIN;
                    softLimitHoldTimer = SOFT_LIMIT_HOLD_TIME_MS;
                }
            }

            // D. Input Brownout Regulation (Soft Floor)
            int64_t error_VinMin = (int64_t)m->voltageIn_mV - limits->vInMin_mV;
            if (error_VinMin < 0) {
                int64_t delta_VinMin = (int64_t)GAIN_KI * error_VinMin * 2; 
                if (delta_VinMin < min_delta) {
                    min_delta = delta_VinMin;
                    activeSoftLimit = LIMIT_V_IN_MIN;
                    softLimitHoldTimer = SOFT_LIMIT_HOLD_TIME_MS;
                }
            }

            // E. Input Voltage Max Limit (Reverse CV)
            int64_t error_VinMax = (int64_t)limits->vInMax_mV - m->voltageIn_mV;
            if (error_VinMax < 0) {
                int64_t delta_VinMax = (int64_t)GAIN_KI * error_VinMax * 2;
                if (delta_VinMax < min_delta) {
                    min_delta = delta_VinMax;
                    activeSoftLimit = LIMIT_V_IN_MAX;
                    softLimitHoldTimer = SOFT_LIMIT_HOLD_TIME_MS;
                }
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

    // 2. Fault Indication
    if (currentState == STATE_FAULT) {
        handleFaultBlink(currentFaultReason);
    } else if (currentState == STATE_ACTIVE) {
        // Heartbeat or active indicator (slow blink)
        if (currentTick % 2000 < 100) {
            HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);
        } else {
            HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
        }
    }

    // 3. Sweeping Logic (Managed here as it's a global search)
    if (currentState == STATE_SWEEPING) {
        if (currentTick - lastSweepStepTick >= SWEEP_STEP_INTERVAL_MS) {
            lastSweepStepTick = currentTick;
            bool finished = false;
            targetDuty_ticks = MPPT_RunSweep(m, limits, &finished);
            if (finished) {
                transitionTo(STATE_ACTIVE);
                lastSweepTick = currentTick;
            }
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
        case STATE_ACTIVE: {
            if (activeSoftLimit == LIMIT_V_OUT_MAX) return "ACTIVE_CV";
            if (activeSoftLimit == LIMIT_I_OUT_MAX) return "ACTIVE_CC";
            if (activeSoftLimit == LIMIT_V_IN_MIN)  return "ACTIVE_BROWNOUT";
            if (activeSoftLimit == LIMIT_V_IN_MAX)  return "ACTIVE_VIN_LIMIT";
            if (activeSoftLimit == LIMIT_I_OUT_MIN) return "ACTIVE_REVERSE";
            return "ACTIVE_TRACKING";
        }
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
