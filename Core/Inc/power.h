/**
  ******************************************************************************
  * @file           : power.h
  * @brief          : Header for power.c file.
  ******************************************************************************
  */

#ifndef __POWER_H
#define __POWER_H

#include "system_types.h"

#ifndef NATIVE_TEST
#include "stm32f0xx_hal.h"
#endif

/**
 * @brief Initialize the power stage and PWM timers.
 */
void POWER_Init(void);

/**
 * @brief Calculate the next duty cycle using the PI algorithm.
 */
void POWER_PID_Compute(PID_t *pid);

/**
 * @brief Set the PWM duty cycle in raw ticks.
 */
void POWER_PWM_Set(int32_t dutyCycleTicks);

/**
 * @brief Get the current logical duty cycle in raw ticks.
 */
int32_t POWER_PWM_Get(void);

/**
 * @brief Get the current logical duty cycle as a percentage (x100).
 */
int32_t POWER_PWM_GetDutyCycle_x100(void);

/**
 * @brief Get the maximum allowed duty cycle in raw ticks.
 */
int32_t POWER_PWM_GetMax(void);

/**
 * @brief Completely disables all PWM outputs and puts the power stage in a safe, disconnected state.
 *        This function stops all timers and complementary outputs.
 */
void POWER_Shutdown(void);

/**
 * @brief Re-initializes and starts the PWM timers, DMA, and complementary outputs.
 *        Call this when transitioning from IDLE/FAULT to an active tracking state.
 */
void POWER_Start(void);

#endif /* __POWER_H */
