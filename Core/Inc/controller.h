/**
  ******************************************************************************
  * @file           : controller.h
  * @brief          : Header for the system state machine and high-level logic.
  ******************************************************************************
  */

#ifndef __CONTROLLER_H
#define __CONTROLLER_H

#include "system_types.h"

/**
 * @brief Initializes the system controller and state machine.
 */
void CONTROLLER_Init(void);

/**
 * @brief High-rate task for safety checks and state transitions.
 *        Called every ADC completion (~200Hz).
 */
void CONTROLLER_UpdateHighRate(void);

/**
 * @brief Timed task for MPPT and telemetry scheduling.
 *        Called every loop iteration, handles its own timing.
 */
void CONTROLLER_Task(void);

/**
 * @brief Returns the current system state.
 */
SystemState_t CONTROLLER_GetState(void);

/**
 * @brief Returns a string representation of the current state.
 */
const char* CONTROLLER_GetStateString(void);

/**
 * @brief Returns a string representation of the current fault reason.
 */
const char* CONTROLLER_GetFaultReasonString(void);

/**
 * @brief Resets the current fault and returns the system to IDLE.
 */
void CONTROLLER_ResetFault(void);

/**
 * @brief Forces a system reset to IDLE state and 0% duty cycle.
 */
void CONTROLLER_Reset(void);

#endif /* __CONTROLLER_H */
