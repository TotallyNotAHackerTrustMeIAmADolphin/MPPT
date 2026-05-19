/**
  ******************************************************************************
  * @file           : mppt.c
  * @brief          : MPPT algorithms implementation.
  ******************************************************************************
  */

#include "mppt.h"
#include "system_config.h"
#include "power.h"
#include <stdlib.h>

/* Private variables */
static int64_t previousPowerIn_uW = 0;
static int32_t previousVoltageIn_mV = 0;
static bool direction = true;

/* Sweep variables */
static int32_t sweepDutyCycle = 0;
static int64_t sweepMaxPower_uW = 0;
static int32_t sweepBestDutyCycle = 0;

int32_t MPPT_PerturbAndObserve(const Measurements_t *m, const DeviceLimits_t *limits) {
    int32_t currentDuty = POWER_PWM_Get();

    // Supply-Aware Protection (Brownout prevention)
    if (m->voltageIn_mV < MIN_INPUT_VOLTAGE_MPPT_MV) {
        currentDuty -= 38; // Back off faster to prevent source collapse
        if (currentDuty < 0) currentDuty = 0;
        previousPowerIn_uW = m->powerIn_uW;
        previousVoltageIn_mV = m->voltageIn_mV;
        return currentDuty;
    }

    int64_t dP = m->powerIn_uW - previousPowerIn_uW;
    int32_t dV = m->voltageIn_mV - previousVoltageIn_mV;
    int32_t adaptiveStep = VSS_MIN_STEP;

    // Calculate Adaptive Step Size based on P-V Slope (|dP/dV|)
    if (abs(dV) > VSS_VOLTAGE_DEADBAND) {
        // Step = N * |dP / dV|
        // To maintain precision in integer math, we work in mW/mV
        int32_t dP_mW = (int32_t)(dP / 1000);
        adaptiveStep = (abs(dP_mW) * VSS_N_FACTOR) / abs(dV);
        
        // Safety bounds
        if (adaptiveStep < VSS_MIN_STEP) adaptiveStep = VSS_MIN_STEP;
        if (adaptiveStep > VSS_MAX_STEP) adaptiveStep = VSS_MAX_STEP;
    }

    // Standard P&O Direction Logic
    if (abs((int32_t)(dP / 1000)) > (POWER_THRESHOLD_UW / 1000)) {
        if (dP < 0) {
            direction = !direction;
        }
        previousPowerIn_uW = m->powerIn_uW;
        previousVoltageIn_mV = m->voltageIn_mV;
    }

    if (direction) {
        currentDuty += adaptiveStep;
    } else {
        currentDuty -= adaptiveStep;
    }

    return currentDuty;
}

int32_t MPPT_RunSweep(const Measurements_t *m, bool *isFinished) {
    *isFinished = false;

    // Supply-Aware Termination
    if (m->voltageIn_mV < MIN_INPUT_VOLTAGE_MPPT_MV) {
        *isFinished = true;
        return sweepBestDutyCycle;
    }

    if (m->powerIn_uW > sweepMaxPower_uW) {
        sweepMaxPower_uW = m->powerIn_uW;
        sweepBestDutyCycle = sweepDutyCycle;
    }

    sweepDutyCycle += SWEEP_STEP_SIZE_TICKS;

    if (sweepDutyCycle > POWER_PWM_GetMax()) {
        *isFinished = true;
        return sweepBestDutyCycle;
    }

    return sweepDutyCycle;
}

void MPPT_ResetSweep(void) {
    sweepDutyCycle = 0; // Will be initialized by controller to minDuty
    sweepMaxPower_uW = 0;
    sweepBestDutyCycle = 0;
}

void MPPT_StartTracking(const Measurements_t *m) {
    previousPowerIn_uW = m->powerIn_uW;
    previousVoltageIn_mV = m->voltageIn_mV;
}
