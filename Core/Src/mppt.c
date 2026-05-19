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
static int32_t vss_n_factor = VSS_N_FACTOR;
static int32_t vss_min_step = VSS_MIN_STEP;
static int32_t vss_max_step = VSS_MAX_STEP;
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

    // 2. Adaptive Step Size calculation
    static int64_t lastFramePower = 0;
    int64_t inst_dP = m->powerOut_uW - lastFramePower;
    int32_t inst_dP_mW = abs((int32_t)(inst_dP / 1000));
    lastFramePower = m->powerOut_uW;

    int32_t adaptiveStep = vss_min_step;

    if (inst_dP > 0) {
        // Power is increasing: Search mode
        // Scale by N_FACTOR (higher = more aggressive)
        adaptiveStep = vss_min_step + (inst_dP_mW * vss_n_factor) / 100;
        if (adaptiveStep > vss_max_step) adaptiveStep = vss_max_step;
    } else {
        // Power is flat or dropping: Micro-stepping
        adaptiveStep = vss_min_step;
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
    previousPowerIn_uW = m->powerIn_uW;
}

int32_t MPPT_GetLastStep(void) {
    return lastStep;
}

void MPPT_SetNFactor(int32_t n) { vss_n_factor = n; }
void MPPT_SetMinStep(int32_t step) { vss_min_step = step; }
void MPPT_SetMaxStep(int32_t step) { vss_max_step = step; }
void MPPT_SetThreshold(uint32_t uw) { pwr_threshold = uw; }
void MPPT_SetInterval(uint32_t ms) { mppt_interval = ms; }

int32_t MPPT_GetNFactor(void) { return vss_n_factor; }
int32_t MPPT_GetMinStep(void) { return vss_min_step; }
int32_t MPPT_GetMaxStep(void) { return vss_max_step; }
uint32_t MPPT_GetThreshold(void) { return pwr_threshold; }
uint32_t MPPT_GetInterval(void) { return mppt_interval; }
