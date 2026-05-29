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

    // 2. Fixed Step Delta (ticks)
    int32_t delta = direction ? mppt_step_size : -mppt_step_size;

    lastStep = mppt_step_size;
    return delta;
}



int32_t MPPT_RunSweep(const Measurements_t *m, const DeviceLimits_t *limits, bool *isFinished) {
    *isFinished = false;

    // Supply-Aware Termination
    if (m->voltageIn_mV < limits->vInMin_mV) {
        *isFinished = true;
        return sweepBestDutyCycle;
    }

    // Limit-Aware Termination (Stop sweep if we hit max voltage or max current)
    if (m->voltageOut_mV >= limits->vOutMax_mV || 
        m->currentOut_mA >= limits->iOutMax_mA ||
        m->currentOut_mA <= limits->iOutMin_mA ||
        m->voltageIn_mV >= limits->vInMax_mV) {
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

void MPPT_ResetSweep(int32_t startDuty) {
    sweepDutyCycle = startDuty; // Start from voltage-matched duty to prevent reverse current
    sweepMaxPower_uW = 0;
    sweepBestDutyCycle = startDuty;
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
