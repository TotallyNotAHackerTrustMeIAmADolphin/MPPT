/**
  ******************************************************************************
  * @file           : settings.c
  * @brief          : Persistent settings and calibration implementation.
  ******************************************************************************
  */

#include "settings.h"
#include "eeprom.h"
#include "main.h"

/* Private variables */
static Calibration_t cal = {
    951, 10000, 2684, 30000,
    892, 10000, 2730, 30000,
    1988, 23, 1675, 3900,
    1986, 0, 1661, 3900
};

static DeviceLimits_t limits = {25200, 18000, 2000, 80000, 20000};
static bool isCalibrating = false;
static bool calHighSideOn = false;

void SETTINGS_Init(void) {
    if (EE_Init() != HAL_OK) return;
  
    // Load Calibration (virtual addresses 1-16)
    uint16_t *pCal = (uint16_t *)&cal;
    for (uint16_t i = 0; i < 16; i++) {
        uint16_t val;
        if (EE_ReadVariable(i + 1, &val) == 0) {
            pCal[i] = val;
        }
    }

    // Load Device Limits (virtual addresses 20-29)
    uint16_t *pLimits = (uint16_t *)&limits;
    for (uint16_t i = 0; i < 10; i++) {
        uint16_t val;
        if (EE_ReadVariable(i + 20, &val) == 0) {
            pLimits[i] = val;
        }
    }
}

void SETTINGS_SaveCalibration(void) {
    uint16_t *pCal = (uint16_t *)&cal;
    for (uint16_t i = 0; i < 16; i++) {
        EE_WriteVariable(i + 1, pCal[i]);
    }
}

void SETTINGS_SaveLimits(void) {
    uint16_t *pLimits = (uint16_t *)&limits;
    for (uint16_t i = 0; i < 10; i++) {
        EE_WriteVariable(i + 20, pLimits[i]);
    }
}

Calibration_t* SETTINGS_GetCalibration(void) {
    return &cal;
}

DeviceLimits_t* SETTINGS_GetLimits(void) {
    return &limits;
}

bool SETTINGS_IsCalibrating(void) {
    return isCalibrating;
}

void SETTINGS_SetCalibrating(bool active) {
    isCalibrating = active;
}

bool SETTINGS_IsCalHighSideOn(void) {
    return calHighSideOn;
}

void SETTINGS_SetCalHighSideOn(bool active) {
    calHighSideOn = active;
}
