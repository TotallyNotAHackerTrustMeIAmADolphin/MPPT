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
static uint32_t lastMPPTTick = 0;
static uint32_t lastSweepTick = 0;
static uint32_t lastTelemetryTick = 0;

static PID_t pidCV;
static PID_t pidCC;
static int32_t targetDuty_ticks = 0;

/* Internal State Transition Logic */
static void transitionTo(SystemState_t newState) {
    if (currentState == newState) return;
    
    // Reset timers when entering certain states
    if (newState == STATE_SWEEPING) {
        MPPT_ResetSweep();
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
    lastMPPTTick = 0;
    lastSweepTick = 0;
    lastTelemetryTick = 0;

    const DeviceLimits_t *limits = SETTINGS_GetLimits();
    const Measurements_t *m = SENSORS_GetMeasurements();

    // Initialize PID controllers
    pidCV.Kp = 100; pidCV.Ki = 0; pidCV.Kd = 10;
    pidCV.previousError = 0; pidCV.integral = 0;
    pidCV.setPoint = limits->batteryMax_mV;
    pidCV.input = (int32_t*)&m->voltageOut_mV;
    pidCV.output = &targetDuty_ticks;
    pidCV.maxIntegral = 1000000;
    pidCV.minOutput = 0;
    pidCV.maxOutput = POWER_PWM_GetMax();

    pidCC.Kp = 100; pidCC.Ki = 0; pidCC.Kd = 10;
    pidCC.previousError = 0; pidCC.integral = 0;
    pidCC.setPoint = limits->chargingCurrent_mA;
    pidCC.input = (int32_t*)&m->currentOut_mA;
    pidCC.output = &targetDuty_ticks;
    pidCC.maxIntegral = 1000000;
    pidCC.minOutput = 0;
    pidCC.maxOutput = POWER_PWM_GetMax();
}

void CONTROLLER_UpdateHighRate(void) {
    const Measurements_t *m = SENSORS_GetMeasurements();
    const DeviceLimits_t *limits = SETTINGS_GetLimits();
    uint32_t currentTick = HAL_GetTick();

    // 1. Safety Logic (Highest Priority)
    bool fault = (m->voltageIn_mV > limits->inputVoltageMax_mV) ||
                 (m->currentIn_mA > limits->inputCurrentMax_mA) ||
                 (m->voltageOut_mV > (limits->batteryMax_mV + 500)); // Hard over-voltage

    if (fault) {
        transitionTo(STATE_FAULT);
    }

    // 2. State-Specific High-Rate Logic
    switch (currentState) {
        case STATE_FAULT:
            targetDuty_ticks = 0;
            break;

        case STATE_IDLE:
            targetDuty_ticks = 0;
            if (m->voltageIn_mV > MIN_VOLTAGE_IN_MV) {
                transitionTo(STATE_SWEEPING);
            }
            break;

        case STATE_CV:
            POWER_PID_Compute(&pidCV);
            if (m->voltageOut_mV < (limits->batteryMax_mV - 100)) {
                transitionTo(STATE_MPPT);
            }
            break;

        case STATE_CC:
            POWER_PID_Compute(&pidCC);
            if (m->currentOut_mA < (limits->chargingCurrent_mA - 100)) {
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
        targetDuty_ticks = SETTINGS_IsCalHighSideOn() ? POWER_PWM_GetMax() : 0;
    }

    POWER_PWM_Set(targetDuty_ticks);
}

void CONTROLLER_Task(void) {
    uint32_t currentTick = HAL_GetTick();
    const Measurements_t *m = SENSORS_GetMeasurements();

    // 1. Telemetry (10Hz)
    if (currentTick - lastTelemetryTick >= TELEMETRY_INTERVAL_MS) {
        lastTelemetryTick = currentTick;
        COMMS_SendTelemetry(m);
    }

    // 2. Control Logic (Timed)
    if (currentTick - lastMPPTTick >= MPPT_INTERVAL_MS) {
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
                    targetDuty_ticks = MPPT_PerturbAndObserve(m);
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
