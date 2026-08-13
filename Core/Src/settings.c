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
// Factory defaults, computed from v1.3 hardware specs (not bench-measured) so a
// fresh board reads roughly correct values before real calibration is run:
//
// Voltage (Vin/Vout, identical R5/R6 and R13/R14 200k/4.7k dividers - see
// CALCULATIONS.typ "Voltage Divider & ADC Scaling"):
//   raw = V_real_mV * (4.7/204.7) / 3300mV * 4096  (12-bit ADC, VDDA ~= 3300mV)
//   -> 285 @ 10V, 855 @ 30V
//
// Current (Iin/Iout, CC6937S8-3FB020, 66mV/A, VOUTQ = VCC/2 = 1.65V @ 0A - see
// CALCULATIONS.typ "Current Sense"):
//   raw = V_adc_mV / 3300mV * 4096
//   -> 2048 @ 0A. The 1229 @ 10A point assumes higher current reads as a LOWER
//   raw value, matching this field's previous (bench-measured) defaults' slope
//   direction - NOT independently verified against the physical board. If real
//   calibration ever shows currents reading with the wrong sign, see
//   CALCULATIONS.typ "Factory Calibration Defaults" for how to recompute the
//   high point correctly (not just swapping these two numbers).
static Calibration_t cal = {
    285, 10000, 855, 30000,
    285, 10000, 855, 30000,
    2048, 0, 1229, 10000,
    2048, 0, 1229, 10000
};

static DeviceLimits_t limits = {
    .mode = MODE_MPPT,
    .vOutMax_mV = 24000,
    .iOutMax_mA = 2000,
    .vInMin_mV = 14000,
    .vInMax_mV = 80000,
    .iInMin_mA = -500,
    .iOutMin_mA = -500
};
static bool isCalibrating = false;
static bool calHighSideOn = false;

#define SETTINGS_SIGNATURE 0xABCD
#define LIMITS_WORDS (sizeof(DeviceLimits_t) / 2)

void SETTINGS_Init(void) {
    if (EE_Init() != HAL_OK) return;
  
    uint16_t signature;
    if (EE_ReadVariable(0, &signature) != 0 || signature != SETTINGS_SIGNATURE) {
        // EEPROM is empty or invalid, keep defaults and save them
        SETTINGS_SaveCalibration();
        SETTINGS_SaveLimits();
        EE_WriteVariable(0, SETTINGS_SIGNATURE);
        return;
    }

    // Load Calibration (virtual addresses 1-16)
    uint16_t *pCal = (uint16_t *)&cal;
    for (uint16_t i = 0; i < 16; i++) {
        uint16_t val;
        if (EE_ReadVariable(i + 1, &val) == 0) {
            pCal[i] = val;
        }
    }

    // Load Device Limits (virtual addresses 20-35 reserved)
    uint16_t *pLimits = (uint16_t *)&limits;
    for (uint16_t i = 0; i < LIMITS_WORDS; i++) {
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
    for (uint16_t i = 0; i < LIMITS_WORDS; i++) {
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
