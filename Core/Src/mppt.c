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
static int32_t lastStep = 0;

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

        // BREAK THE TRAP: Explicitly force direction to "decrease duty/increase voltage"
        direction = false;

        previousPowerIn_uW = m->powerOut_uW;
        return currentDuty;
    }

    // Calculate change relative to the last STABLE baseline
    int64_t dP = m->powerOut_uW - previousPowerIn_uW;
    int32_t dP_mW = (int32_t)(dP / 1000);

    // 1. Direction Logic: Only act if total change > threshold
    if (abs(dP_mW) >= (POWER_THRESHOLD_UW / 1000)) {
        if (dP < 0) {
            // Power dropped significantly: Reverse!
            direction = !direction;
        }
        // Update baseline
        previousPowerIn_uW = m->powerOut_uW;
    }

    // 2. Adaptive Step Size: Proportional to dP
    // We use a slow scaling factor because the 100ms loop allows larger settling
    static int64_t lastFramePower = 0;
    int64_t instantaneous_dP = m->powerOut_uW - lastFramePower;
    int32_t instantaneous_dP_mW = abs((int32_t)(instantaneous_dP / 1000));
    lastFramePower = m->powerOut_uW;

    int32_t adaptiveStep = VSS_MIN_STEP;

    if (instantaneous_dP > 0) {
        // Power is increasing: High-gain Search mode
        adaptiveStep = VSS_MIN_STEP + (instantaneous_dP_mW / 5);
        if (adaptiveStep > VSS_MAX_STEP) adaptiveStep = VSS_MAX_STEP;
    } else {
        // Power is flat or dropping: Micro-stepping (2 ticks)
        adaptiveStep = VSS_MIN_STEP;
    }

    // 3. Apply Perturbation
    if (direction) {
        currentDuty += adaptiveStep;
    } else {
        currentDuty -= adaptiveStep;
    }

    // 4. Safety Cap: Limit MPPT to 95% duty cycle
    if (currentDuty > 1824) currentDuty = 1824;

    lastStep = adaptiveStep;
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
    previousPowerIn_uW = m->powerOut_uW;
    previousVoltageIn_mV = m->voltageIn_mV;
}

int32_t MPPT_GetLastStep(void) {
    return lastStep;
}
