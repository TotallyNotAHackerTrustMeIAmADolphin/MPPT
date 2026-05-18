/**
  ******************************************************************************
  * @file           : sensors.h
  * @brief          : Header for sensor data acquisition and processing.
  ******************************************************************************
  */

#ifndef __SENSORS_H
#define __SENSORS_H

#include "system_types.h"

/**
 * @brief Initializes the sensor module and starts ADC DMA.
 */
void SENSORS_Init(void);

/**
 * @brief Processes raw ADC data from a buffer half.
 * @param offset Starting index in the ADC DMA buffer.
 */
void SENSORS_Process(uint16_t offset);

/**
 * @brief Returns the most recent physical measurements.
 */
const Measurements_t* SENSORS_GetMeasurements(void);

/**
 * @brief Returns the raw ADC sums for calibration purposes.
 */
void SENSORS_GetRawSums(uint32_t *vIn, uint32_t *vOut, uint32_t *aIn, uint32_t *aOut);

/**
 * @brief Checks if a new buffer half is ready for processing.
 * @return 0 if idle, 1 for first half, 2 for second half.
 */
uint8_t SENSORS_IsBufferReady(void);

/**
 * @brief Clears the buffer ready flag.
 */
void SENSORS_ClearBufferReady(void);

#endif /* __SENSORS_H */
