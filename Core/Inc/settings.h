/**
  ******************************************************************************
  * @file           : settings.h
  * @brief          : Header for persistent settings and calibration storage.
  ******************************************************************************
  */

#ifndef __SETTINGS_H
#define __SETTINGS_H

#include "system_types.h"

/**
 * @brief Initializes the settings module and loads from EEPROM.
 */
void SETTINGS_Init(void);

/**
 * @brief Saves the current calibration to EEPROM.
 */
void SETTINGS_SaveCalibration(void);

/**
 * @brief Saves the current device limits to EEPROM.
 */
void SETTINGS_SaveLimits(void);

/**
 * @brief Returns a pointer to the global calibration structure.
 */
Calibration_t* SETTINGS_GetCalibration(void);

/**
 * @brief Returns a pointer to the global device limits structure.
 */
DeviceLimits_t* SETTINGS_GetLimits(void);

/**
 * @brief Returns the calibration mode flags.
 */
bool SETTINGS_IsCalibrating(void);
void SETTINGS_SetCalibrating(bool active);
bool SETTINGS_IsCalHighSideOn(void);
void SETTINGS_SetCalHighSideOn(bool active);

#endif /* __SETTINGS_H */
