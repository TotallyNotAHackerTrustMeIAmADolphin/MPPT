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
    
    // We want to evaluate: dI/dV == -I/V 
    // To avoid division by zero and floating point math, we cross-multiply:
    // dI * V == -I * dV
    
    if (dV == 0) {
        // Pure irradiance change (no voltage change)
        if (dI == 0) {
            // Stable
            delta = 0;
        } else if (dI > 0) {
            // Irradiance increased, move left (decrease duty cycle to increase Vin)
            delta = -mppt_step_size;
        } else {
            // Irradiance decreased, move right
            delta = mppt_step_size;
        }
    } else {
        // Standard IncCond evaluation
        int64_t di_v = (int64_t)dI * m->voltageIn_mV;
        int64_t i_dv = (int64_t)m->currentIn_mA * dV;
        
        if (di_v == -i_dv) {
            // At MPP
            delta = 0;
        } else if (di_v > -i_dv) {
            // Left of MPP: decrease duty cycle to increase Vin
            delta = -mppt_step_size;
        } else {
            // Right of MPP: increase duty cycle to decrease Vin
            delta = mppt_step_size;
        }
    }
    
    // In our topology, increasing duty cycle drops Vin (pulls more power)
    // Wait, let's verify our control direction from P&O:
    // P&O changes duty. If power went up, we keep same delta.
    // If we are left of MPP (too high voltage), we need to pull more current -> increase duty.
    // So if di/dV > -I/V, we are left of MPP. Voltage is too high. 
    // Wait, the solar panel curve: 
    // V=Voc (open circuit), I=0. Left of MPP means V is too low?
    // Let's standardise: Panel voltage V. 
    // Left of MPP: V < Vmpp. Right of MPP: V > Vmpp.
    // If di_v > -i_dv -> dI/dV > -I/V. This occurs when V < Vmpp.
    // If V < Vmpp (left), we need to INCREASE Vin -> DECREASE duty cycle.
    // If V > Vmpp (right), we need to DECREASE Vin -> INCREASE duty cycle.
    // Wait, the delta in code above: di_v > -i_dv means left (V < Vmpp) -> delta = -mppt_step_size. Correct.
    
    // Only update baselines if we actually moved, or if the change was significant
    if (abs((int)(m->powerOut_uW - previousPowerIn_uW)) / 1000 >= (int32_t)(pwr_threshold / 1000) || delta == 0) {
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
