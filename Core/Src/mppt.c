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

/* IncCond State Variables */
static int32_t previousVoltageIn_mV = 0;
static int32_t previousCurrentIn_mA = 0;

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



int32_t MPPT_IncrementalConductance(const Measurements_t *m, const DeviceLimits_t *limits) {
    int32_t delta = 0;
    
    int32_t dV = m->voltageIn_mV - previousVoltageIn_mV;
    int32_t dI = m->currentIn_mA - previousCurrentIn_mA;
    
    /* 
     * Incremental Conductance (IncCond) Logic:
     * The MPP is found when dP/dV = 0.
     * Since P = V * I, then dP/dV = I + V * (dI/dV).
     * At MPP: dI/dV = -I/V.
     * To avoid division by zero and floating point math, we evaluate the slope 
     * using the cross-product: slope_num = (dI * V) + (I * dV).
     */
    
    if (dV == 0) {
        /* 
         * Case 1: Voltage has not changed.
         * If current has changed, it is due to an irradiance change.
         */
        if (dI == 0) {
            delta = 0; // Stable
        } else if (dI > 0) {
            /* 
             * Irradiance increased, MPP moved right (higher current capacity). 
             * Increase duty cycle to pull more power and follow the peak.
             */
            delta = mppt_step_size;
        } else {
            /* 
             * Irradiance decreased, panel is being choked. 
             * Move left (decrease duty) to recover voltage.
             */
            delta = -mppt_step_size;
        }
    } else {
        /* 
         * Case 2: Voltage has changed. Evaluate incremental conductance.
         * slope_num is proportional to dP/dV.
         */
        int64_t slope_num = (int64_t)dI * m->voltageIn_mV + (int64_t)m->currentIn_mA * dV;
        
        if (slope_num == 0) {
            delta = 0; // At MPP peak
        } else {
            /* 
             * Sign Handling Logic:
             * In our topology, Increasing Duty -> Decreases Vin.
             * 
             * If dV > 0 (Voltage increased):
             *   If slope_num > 0 (dP/dV > 0): Left of peak -> Increase V further (Decrease Duty)
             *   If slope_num < 0 (dP/dV < 0): Right of peak -> Decrease V (Increase Duty)
             * 
             * If dV < 0 (Voltage decreased):
             *   If slope_num > 0 (dP/dV > 0): Right of peak -> Decrease V further (Increase Duty)
             *   If slope_num < 0 (dP/dV < 0): Left of peak -> Increase V (Decrease Duty)
             */
            if (dV > 0) {
                delta = (slope_num > 0) ? -mppt_step_size : mppt_step_size;
            } else {
                delta = (slope_num > 0) ? mppt_step_size : -mppt_step_size;
            }
        }
    }
    
    /* 
     * Update baselines if the change was significant enough to be real signal,
     * or if we have reached a stable equilibrium (delta == 0).
     */
    if (abs((int)dV) > 50 || abs((int)dI) > 50 || delta == 0) {
        previousVoltageIn_mV = m->voltageIn_mV;
        previousCurrentIn_mA = m->currentIn_mA;
        previousPowerIn_uW = m->powerOut_uW;
    }
    
    lastStep = delta;
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
    previousVoltageIn_mV = m->voltageIn_mV;
    previousCurrentIn_mA = m->currentIn_mA;
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
