/**
  ******************************************************************************
  * @file           : power.h
  * @brief          : Header for power control, PWM, and PID regulation.
  ******************************************************************************
  */

#ifndef __POWER_H
#define __POWER_H

#include "system_types.h"

/**
 * @brief Initializes the power module and starts PWM timers.
 */
void POWER_Init(void);

/**
 * @brief Sets the raw dithered duty cycle in ticks.
 * @param dutyCycleTicks Total duty cycle (0 to MAX_DUTY_CYCLE_TICKS).
 */
void POWER_PWM_Set(int32_t dutyCycleTicks);

/**
 * @brief Executes a PID step and updates the output variable.
 * @param pid Pointer to the PID state structure.
 */
void POWER_PID_Compute(PID_t *pid);

/**
 * @brief Returns the current raw duty cycle in ticks.
 */
int32_t POWER_PWM_Get(void);

/**
 * @brief Returns the maximum possible duty cycle ticks.
 */
int32_t POWER_PWM_GetMax(void);

#endif /* __POWER_H */
