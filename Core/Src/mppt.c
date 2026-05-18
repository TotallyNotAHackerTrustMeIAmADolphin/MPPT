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
        return currentDuty;
    }

    // Regulation-Aware Protection (Soft Ceiling)
    // If we are within 150mV of the limit, do not increase duty
    bool nearLimit = (m->voltageOut_mV > (limits->batteryMax_mV - 150)) ||
                     (m->currentOut_mA > (limits->chargingCurrent_mA - 100));

    int64_t powerChange_uW = m->powerIn_uW - previousPowerIn_uW;

    if (abs((int32_t)(powerChange_uW / 1000)) > (POWER_THRESHOLD_UW / 1000)) {
        if (powerChange_uW < 0) {
            direction = !direction;
        }
    }

    if (direction) {
        // Only increase if not near limit
        if (!nearLimit) {
            currentDuty += MPPT_STEP_SIZE_TICKS;
        }
    } else {
        // Decrease faster if near limit to stay out of CV/CC flapping
        currentDuty -= nearLimit ? (MPPT_STEP_SIZE_TICKS * 2) : MPPT_STEP_SIZE_TICKS;
    }

    previousPowerIn_uW = m->powerIn_uW;
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
