/**
  ******************************************************************************
  * @file           : power.c
  * @brief          : Power control, PWM, and PID regulation implementation.
  ******************************************************************************
  */

#include "power.h"
#include "system_config.h"
#ifndef NATIVE_TEST
#include "tim.h"
#endif
#include <string.h>

/* Private constants */
#define MAX_DUTY_CYCLE_TICKS (TIMER_PERIOD * DITHER_TABLE_SIZE)
#define PWM_DEAD_BAND_TICKS  ((MAX_DUTY_CYCLE_TICKS * 2) / 100) // 2% of range

// v1.3 has no isolated high-side gate supply - only bootstrap caps, which
// recharge solely while their leg's low side conducts. Pinning a leg's high
// side to a literal 100% (as a naive buck/boost map would during sustained
// single-mode operation) starves that leg's bootstrap cap indefinitely.
// CEILING_TICKS reserves a guaranteed low-side window every single PWM cycle
// on whichever leg is "fixed" - in deep buck, deep boost, at crossover, and
// in calibration mode alike - by acting as the reference for the hardware
// map below (not just an output clamp). Must clear 2x dead-time
// (TIM1.DeadTime, see tim.c) with margin for actual recharge current, not
// just break-even; start conservative and tune from bench measurement of
// bootstrap cap voltage under sustained single-stage operation.
#define BOOTSTRAP_REFRESH_TICKS ((MAX_DUTY_CYCLE_TICKS * 6) / 100) // ~6% reserve to start
#define CEILING_TICKS (MAX_DUTY_CYCLE_TICKS - BOOTSTRAP_REFRESH_TICKS)

// If the reserve ever grows too large, CH1Value = 2*CEILING_TICKS - hardwareDuty
// goes negative at the deep-boost extreme (hardwareDuty == maxDutyCycle_ticks,
// 170% of MAX_DUTY_CYCLE_TICKS below).
_Static_assert(2 * CEILING_TICKS >= (MAX_DUTY_CYCLE_TICKS * 170) / 100,
               "BOOTSTRAP_REFRESH_TICKS too large: CH1Value would go negative at max boost duty");

// ditherBits must match DITHER_TABLE_SIZE exactly - see updateDitherTable().
#define DITHER_BITS_LOG2 4
_Static_assert((1 << DITHER_BITS_LOG2) == DITHER_TABLE_SIZE,
               "DITHER_BITS_LOG2 out of sync with DITHER_TABLE_SIZE");

/* Private variables (Encapsulated) */
static uint16_t ditherTableCH1[DITHER_TABLE_SIZE];
static uint16_t ditherTableCH2[DITHER_TABLE_SIZE];
static int32_t currentDutyCycle_ticks = 0;
static const int32_t maxDutyCycle_ticks = (MAX_DUTY_CYCLE_TICKS * 170) / 100;

/* Private function prototypes */
static void updateDitherTable(uint16_t *pDitherTable, uint16_t desiredDutyCycle);

void POWER_Init(void) {
    // Initialize dither tables to zero
    memset(ditherTableCH1, 0, sizeof(ditherTableCH1));
    memset(ditherTableCH2, 0, sizeof(ditherTableCH2));

    // Hardware is initialized but kept in shutdown state initially
    POWER_Shutdown();
}

void POWER_Start(void) {
    // 1. Reset logical state
    currentDutyCycle_ticks = 0;
    memset(ditherTableCH1, 0, sizeof(ditherTableCH1));
    memset(ditherTableCH2, 0, sizeof(ditherTableCH2));

#ifndef NATIVE_TEST
    // 2. Enable Main Output (MOE bit)
    __HAL_TIM_MOE_ENABLE(&htim1);

    // 3. Start PWM with DMA for dithering
    HAL_TIM_PWM_Start_DMA(&htim1, TIM_CHANNEL_1, (uint32_t *)ditherTableCH1, DITHER_TABLE_SIZE);
    HAL_TIM_PWM_Start_DMA(&htim1, TIM_CHANNEL_2, (uint32_t *)ditherTableCH2, DITHER_TABLE_SIZE);

    // 4. Start Complementary PWM outputs
    HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_1);
    HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_2);
#endif

    POWER_PWM_Set(0);
}

void POWER_Shutdown(void) {
    // 1. Force logical duty to 0
    POWER_PWM_Set(0);

#ifndef NATIVE_TEST
    // 2. Stop DMA-driven PWM generation
    HAL_TIM_PWM_Stop_DMA(&htim1, TIM_CHANNEL_1);
    HAL_TIM_PWM_Stop_DMA(&htim1, TIM_CHANNEL_2);

    // 3. Stop Complementary PWM outputs (Very important to stop N-channels)
    HAL_TIMEx_PWMN_Stop(&htim1, TIM_CHANNEL_1);
    HAL_TIMEx_PWMN_Stop(&htim1, TIM_CHANNEL_2);

    // 4. Disable Main Output (MOE bit) for hardware-level safety
    __HAL_TIM_MOE_DISABLE(&htim1);
#endif
}

void POWER_PWM_Set(int32_t dutyCycleTicks) {
    uint32_t CH1Value = 0;
    uint32_t CH2Value = 0;

    // 1. Constrain logic for state retention (Logical State)
    if (dutyCycleTicks < 0) {
        dutyCycleTicks = 0;
    } else if (dutyCycleTicks > maxDutyCycle_ticks) {
        dutyCycleTicks = maxDutyCycle_ticks;
    }
    
    // Save the logical state so algorithms like MPPT can "walk" through dead-bands
    currentDutyCycle_ticks = dutyCycleTicks;

    // 2. Hardware-specific constraints for PWM Generation
    int32_t hardwareDuty = dutyCycleTicks;

    // --- CRITICAL SAFETY: Hard-Zero / Shutdown ---
    // If duty is near zero, we must disable outputs entirely to prevent 
    // the synchronous Low-Side FETs from shorting the rails to ground.
    if (hardwareDuty < PWM_DEAD_BAND_TICKS) {
#ifndef NATIVE_TEST
        __HAL_TIM_MOE_DISABLE(&htim1); // Hardware-level high-impedance
#endif
        CH1Value = 0;
        CH2Value = 0;
        updateDitherTable(ditherTableCH1, 0);
        updateDitherTable(ditherTableCH2, 0);
        return;
    }
#ifndef NATIVE_TEST
    else {
        __HAL_TIM_MOE_ENABLE(&htim1); // Re-enable if we are above dead-band
    }
#endif

    // Apply safety dead-bands to top of range
    if (hardwareDuty > maxDutyCycle_ticks - PWM_DEAD_BAND_TICKS) {
        hardwareDuty = maxDutyCycle_ticks - PWM_DEAD_BAND_TICKS;
    }

    // 3. Hardware PWM Map
    // Referencing both channels against CEILING_TICKS (rather than clamping
    // the output of a timerPeriod-referenced map) collapses "both pinned"
    // back to the single crossover point hardwareDuty == CEILING_TICKS,
    // instead of stretching it into a flat, resolution-losing band - while
    // still guaranteeing every value stays <= CEILING_TICKS, so the leg that
    // isn't actively switching always gets its bootstrap refresh window.
    //
    // An earlier version of this map (back when high-side gate drive came
    // from an isolated supply, not bootstrap caps) snapped hardwareDuty to
    // a fixed value across a band around crossover, to avoid flip-flopping
    // a leg between "fully static" and "actively switching". That binary
    // static/switching distinction doesn't exist any more: with bootstrap
    // caps, whichever leg is "pinned" is already switching every single
    // cycle everywhere in the range (its BOOTSTRAP_REFRESH_TICKS-wide
    // low-side notch is unconditional), so crossing CEILING_TICKS only
    // redistributes which leg's notch width tracks the duty command - no
    // new switching edges appear or disappear at the boundary. The snap is
    // deliberately gone: this map is fully continuous end-to-end with no
    // frozen band, so the "other" leg starts walking the instant this one
    // saturates, and MPPT never loses gradient information near crossover.
    if (hardwareDuty == 0) {
        CH1Value = 0;
        CH2Value = 0;
    } else {
        int32_t boostValue = 2 * CEILING_TICKS - hardwareDuty;
        CH1Value = (uint32_t)((boostValue < CEILING_TICKS) ? boostValue : CEILING_TICKS);
        CH2Value = (uint32_t)((hardwareDuty < CEILING_TICKS) ? hardwareDuty : CEILING_TICKS);
    }

    updateDitherTable(ditherTableCH1, (uint16_t)CH1Value);
    updateDitherTable(ditherTableCH2, (uint16_t)CH2Value);
}

void POWER_PID_Compute(PID_t *pid) {
    int32_t input = *pid->input;
    int32_t error = pid->setPoint - input;
    
    // Velocity PID form with Derivative-on-Input
    // delta_output = Ki * error - Kp * (input - previousInput)
    // This form avoids 'proportional kick' when the setpoint changes.
    int64_t delta = (int64_t)pid->Ki * error - (int64_t)pid->Kp * (input - pid->previousInput);

    // Use pid->integral as a high-resolution duty cycle accumulator (x1000)
    pid->integral += delta;

    // Constrain the high-resolution accumulator to physical PWM limits
    int64_t minScaled = (int64_t)pid->minOutput * 1000;
    int64_t maxScaled = (int64_t)pid->maxOutput * 1000;
    
    if (pid->integral < minScaled) pid->integral = minScaled;
    if (pid->integral > maxScaled) pid->integral = maxScaled;

    // Update the actual duty cycle variable
    *pid->output = (int32_t)(pid->integral / 1000);

    pid->previousError = error;
    pid->previousInput = input;
}

int32_t POWER_PWM_Get(void) {
    return currentDutyCycle_ticks;
}

int32_t POWER_PWM_GetDutyCycle_x100(void) {
    return (currentDutyCycle_ticks * 10000) / MAX_DUTY_CYCLE_TICKS;
}

int32_t POWER_PWM_GetMax(void) {
    return maxDutyCycle_ticks;
}

int32_t POWER_CalculateVoltageMatchDuty(int32_t vIn_mV, int32_t vOut_mV) {
    if (vIn_mV <= 100) return 0; // Prevent divide by zero or extreme noise

    // Must match the hardware map's logical crossover point (CEILING_TICKS,
    // not MAX_DUTY_CYCLE_TICKS/timerPeriod) or startup precharge duty is
    // mis-aimed by up to BOOTSTRAP_REFRESH_TICKS, risking a backflow fault.
    int32_t matchDuty = 0;

    if (vOut_mV <= vIn_mV) {
        // Buck mode
        matchDuty = (int32_t)(((int64_t)CEILING_TICKS * vOut_mV) / vIn_mV);
    } else {
        // Boost mode
        matchDuty = CEILING_TICKS + (int32_t)(((int64_t)CEILING_TICKS * (vOut_mV - vIn_mV)) / vOut_mV);
    }
    
    if (matchDuty < 0) matchDuty = 0;
    if (matchDuty > maxDutyCycle_ticks) matchDuty = maxDutyCycle_ticks;
    
    return matchDuty;
}

static void updateDitherTable(uint16_t *pDitherTable, uint16_t desiredDutyCycle) {
    uint16_t baseDutyCycle = desiredDutyCycle >> DITHER_BITS_LOG2;
    uint16_t ditherIndex = desiredDutyCycle & ((1 << DITHER_BITS_LOG2) - 1);

    for (uint16_t i = 0; i < DITHER_TABLE_SIZE; i++) {
        pDitherTable[i] = (i < ditherIndex) ? (baseDutyCycle + 1) : baseDutyCycle;
    }
}
