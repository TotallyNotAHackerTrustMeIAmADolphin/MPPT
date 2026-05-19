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
static bool direction = true;
static int32_t lastStep = 0;

/* Dynamic Tuning Parameters (Defaults from system_config.h) */
static int32_t mppt_step_size = MPPT_STEP_SIZE_TICKS;
static uint32_t pwr_threshold = POWER_THRESHOLD_UW;
static uint32_t mppt_interval = MPPT_INTERVAL_MS;

/* Sweep variables */
static int32_t sweepDutyCycle = 0;
static int64_t sweepMaxPower_uW = 0;
static int32_t sweepBestDutyCycle = 0;

int32_t MPPT_PerturbAndObserve(const Measurements_t *m, const DeviceLimits_t *limits) {
    int32_t currentDuty = POWER_PWM_Get();

    // Supply-Aware Protection (Brownout prevention)
    if (m->voltageIn_mV < MIN_INPUT_VOLTAGE_MPPT_MV) {
        currentDuty -= 38; 
        if (currentDuty < 0) currentDuty = 0;
        direction = false; // Force direction to increase voltage
        previousPowerIn_uW = m->powerOut_uW;
        return currentDuty;
    }

    // Calculate change relative to the last STABLE baseline
    int64_t dP = m->powerOut_uW - previousPowerIn_uW;
    int32_t dP_mW = (int32_t)(dP / 1000);

    // 1. Direction Logic: Only act if total change > threshold
    if (abs(dP_mW) >= (int32_t)(pwr_threshold / 1000)) {
        if (dP < 0) {
            direction = !direction;
        }
        previousPowerIn_uW = m->powerOut_uW;
    }

    // 2. Fixed Step Perturbation
    if (direction) {
        currentDuty += mppt_step_size;
    } else {
        currentDuty -= mppt_step_size;
    }

    // 3. Safety Cap: Limit MPPT to 95% of the absolute maximum duty cycle
    int32_t absoluteMax = POWER_PWM_GetMax();
    int32_t safetyMargin = (absoluteMax * 5) / 100; // 5% margin
    if (currentDuty > absoluteMax - safetyMargin) currentDuty = absoluteMax - safetyMargin;

    lastStep = mppt_step_size;
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
}

int32_t MPPT_GetLastStep(void) {
    return lastStep;
}

void MPPT_SetStepSize(int32_t step) { mppt_step_size = step; }
void MPPT_SetThreshold(uint32_t uw) { pwr_threshold = uw; }
void MPPT_SetInterval(uint32_t ms) { mppt_interval = ms; }

int32_t MPPT_GetStepSize(void) { return mppt_step_size; }
uint32_t MPPT_GetThreshold(void) { return pwr_threshold; }
uint32_t MPPT_GetInterval(void) { return mppt_interval; }
