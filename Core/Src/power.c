/**
  ******************************************************************************
  * @file           : power.c
  * @brief          : Power control, PWM, and PID regulation implementation.
  ******************************************************************************
  */

#include "power.h"
#include "system_config.h"
#include "tim.h"
#include <string.h>

/* Private constants */
#define MAX_DUTY_CYCLE_TICKS (TIMER_PERIOD * DITHER_TABLE_SIZE)
#define PWM_DEAD_BAND_TICKS  ((MAX_DUTY_CYCLE_TICKS * 2) / 100) // 2% of range

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

    // 2. Enable Main Output (MOE bit)
    __HAL_TIM_MOE_ENABLE(&htim1);

    // 3. Start PWM with DMA for dithering
    HAL_TIM_PWM_Start_DMA(&htim1, TIM_CHANNEL_1, (uint32_t *)ditherTableCH1, DITHER_TABLE_SIZE);
    HAL_TIM_PWM_Start_DMA(&htim1, TIM_CHANNEL_2, (uint32_t *)ditherTableCH2, DITHER_TABLE_SIZE);

    // 4. Start Complementary PWM outputs
    HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_1);
    HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_2);

    POWER_PWM_Set(0);
}

void POWER_Shutdown(void) {
    // 1. Force logical duty to 0
    POWER_PWM_Set(0);

    // 2. Stop DMA-driven PWM generation
    HAL_TIM_PWM_Stop_DMA(&htim1, TIM_CHANNEL_1);
    HAL_TIM_PWM_Stop_DMA(&htim1, TIM_CHANNEL_2);

    // 3. Stop Complementary PWM outputs (Very important to stop N-channels)
    HAL_TIMEx_PWMN_Stop(&htim1, TIM_CHANNEL_1);
    HAL_TIMEx_PWMN_Stop(&htim1, TIM_CHANNEL_2);

    // 4. Disable Main Output (MOE bit) for hardware-level safety
    __HAL_TIM_MOE_DISABLE(&htim1);
}

void POWER_PWM_Set(int32_t dutyCycleTicks) {
    int32_t timerPeriod = MAX_DUTY_CYCLE_TICKS;
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
        __HAL_TIM_MOE_DISABLE(&htim1); // Hardware-level high-impedance
        CH1Value = 0;
        CH2Value = 0;
        updateDitherTable(ditherTableCH1, 0);
        updateDitherTable(ditherTableCH2, 0);
        return; 
    } else {
        __HAL_TIM_MOE_ENABLE(&htim1); // Re-enable if we are above dead-band
    }

    // Apply safety dead-bands to top of range
    if (hardwareDuty > maxDutyCycle_ticks - PWM_DEAD_BAND_TICKS) {
        hardwareDuty = maxDutyCycle_ticks - PWM_DEAD_BAND_TICKS;
    }
    
    // Near-passthrough dead-band (prevents switching noise/instability at 100%)
    if (hardwareDuty < timerPeriod && hardwareDuty > timerPeriod - PWM_DEAD_BAND_TICKS) {
        hardwareDuty = timerPeriod;
    } else if (hardwareDuty > timerPeriod && hardwareDuty < timerPeriod + PWM_DEAD_BAND_TICKS) {
        hardwareDuty = timerPeriod;
    }

    // 3. Hardware PWM Map
    if (hardwareDuty == 0) {
        CH1Value = 0;
        CH2Value = 0;
    } else if (hardwareDuty == timerPeriod) {
        CH1Value = timerPeriod;
        CH2Value = timerPeriod;
    } else if (hardwareDuty < timerPeriod) {
        // Buck mode: HS1 fully on, HS2 switching
        CH1Value = timerPeriod;
        CH2Value = hardwareDuty;
    } else {
        // Boost mode: HS1 switching, HS2 fully on
        CH1Value = 2 * timerPeriod - hardwareDuty;
        CH2Value = timerPeriod;
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

static void updateDitherTable(uint16_t *pDitherTable, uint16_t desiredDutyCycle) {
    // ditherBits is 3 (log2 of DITHER_TABLE_SIZE 8)
    const uint8_t ditherBits = 3;
    uint16_t baseDutyCycle = desiredDutyCycle >> ditherBits;
    uint16_t ditherIndex = desiredDutyCycle & ((1 << ditherBits) - 1);

    for (uint16_t i = 0; i < DITHER_TABLE_SIZE; i++) {
        pDitherTable[i] = (i < ditherIndex) ? (baseDutyCycle + 1) : baseDutyCycle;
    }
}
