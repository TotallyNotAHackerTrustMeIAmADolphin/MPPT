/**
  ******************************************************************************
  * @file           : comms.h
  * @brief          : Header for serial communication and telemetry.
  ******************************************************************************
  */

#ifndef __COMMS_H
#define __COMMS_H

#include "system_types.h"

/**
 * @brief Initializes the communication module.
 */
void COMMS_Init(void);

/**
 * @brief Processes incoming serial commands.
 *        Should be called frequently in the main loop.
 */
void COMMS_HandleCommands(void);

/**
 * @brief Sends periodic telemetry data in JSON format.
 * @param measurements Pointer to the current sensor measurements.
 */
void COMMS_SendTelemetry(const Measurements_t *measurements);

#endif /* __COMMS_H */
