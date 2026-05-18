/**
  ******************************************************************************
  * @file           : comms.c
  * @brief          : Serial communication and telemetry implementation.
  ******************************************************************************
  */

#include "comms.h"
#include "controller.h"
#include "power.h"
#include "sensors.h"
#include "settings.h"
#include "usbd_cdc_if.h"
#include "system_config.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Private variables */
static char cmdBuffer[64];
static uint8_t cmdIdx = 0;

void COMMS_Init(void) {
    memset(cmdBuffer, 0, sizeof(cmdBuffer));
    cmdIdx = 0;
}

void COMMS_SendTelemetry(const Measurements_t *m) {
    // Basic telemetry
    printf("{\"type\":\"telemetry\",\"Vin_mV\":%ld,\"Vout_mV\":%ld,\"Ain_mA\":%ld,\"Aout_mA\":%ld,\"Win_mW\":%ld,\"Wout_mW\":%ld,\"duty\":%ld,\"eff\":%d,\"temp_C\":%ld,\"state\":\"%s\",\"fault_reason\":\"%s\"}\n",
           m->voltageIn_mV, m->voltageOut_mV, m->currentIn_mA, m->currentOut_mA,
           m->powerIn_mW, m->powerOut_mW, POWER_PWM_Get(), 
           m->efficiency_x100 / 100, m->tempMCU_C_x100 / 100,
           CONTROLLER_GetStateString(),
           CONTROLLER_GetFaultReasonString());

    // Calibration raw data if active
    if (SETTINGS_IsCalibrating()) {
        uint32_t vIn, vOut, aIn, aOut;
        SENSORS_GetRawSums(&vIn, &vOut, &aIn, &aOut);
        printf("{\"type\":\"cal_raw\",\"Vin_raw\":%lu,\"Vout_raw\":%lu,\"Ain_raw\":%lu,\"Aout_raw\":%lu}\n",
               vIn / (ADC_SAMPLE_COUNT / 2), vOut / (ADC_SAMPLE_COUNT / 2),
               aIn / (ADC_SAMPLE_COUNT / 2), aOut / (ADC_SAMPLE_COUNT / 2));
    }
}

void COMMS_HandleCommands(void) {
    Calibration_t *cal = SETTINGS_GetCalibration();
    DeviceLimits_t *limits = SETTINGS_GetLimits();

    while (CDC_Available()) {
        uint8_t c = CDC_Read();
        if (c == '\n' || c == '\r') {
            if (cmdIdx > 0) {
                cmdBuffer[cmdIdx] = 0;
                
                if (strcmp(cmdBuffer, "CMD:CAL_ENTER") == 0) {
                    SETTINGS_SetCalibrating(true);
                    printf("ACK:CAL_ENTER_OK\n");
                } else if (strcmp(cmdBuffer, "CMD:CAL_EXIT") == 0) {
                    SETTINGS_SetCalibrating(false);
                    SETTINGS_SetCalHighSideOn(false);
                    printf("ACK:CAL_EXIT_OK\n");
                } else if (strcmp(cmdBuffer, "CMD:CAL_MODE_I") == 0) {
                    SETTINGS_SetCalHighSideOn(true);
                    printf("ACK:CAL_MODE_I_OK\n");
                } else if (strcmp(cmdBuffer, "CMD:CAL_MODE_V") == 0) {
                    SETTINGS_SetCalHighSideOn(false);
                    printf("ACK:CAL_MODE_V_OK\n");
                } else if (strncmp(cmdBuffer, "CMD:CAL_I_LOW:", 14) == 0) {
                    uint32_t vIn, vOut, aIn, aOut;
                    SENSORS_GetRawSums(&vIn, &vOut, &aIn, &aOut);
                    cal->aInRawLow = (uint16_t)(aIn / (ADC_SAMPLE_COUNT / 2));
                    cal->aOutRawLow = (uint16_t)(aOut / (ADC_SAMPLE_COUNT / 2));
                    cal->aInRealLow_mA = cal->aOutRealLow_mA = (uint16_t)atoi(cmdBuffer + 14);
                    printf("ACK:CAL_I_LOW_OK:%d,%d\n", cal->aInRawLow, cal->aOutRawLow);
                } else if (strncmp(cmdBuffer, "CMD:CAL_I_HIGH:", 15) == 0) {
                    uint32_t vIn, vOut, aIn, aOut;
                    SENSORS_GetRawSums(&vIn, &vOut, &aIn, &aOut);
                    cal->aInRawHigh = (uint16_t)(aIn / (ADC_SAMPLE_COUNT / 2));
                    cal->aOutRawHigh = (uint16_t)(aOut / (ADC_SAMPLE_COUNT / 2));
                    cal->aInRealHigh_mA = cal->aOutRealHigh_mA = (uint16_t)atoi(cmdBuffer + 15);
                    printf("ACK:CAL_I_HIGH_OK:%d,%d\n", cal->aInRawHigh, cal->aOutRawHigh);
                } else if (strncmp(cmdBuffer, "CMD:CAL_V_LOW:", 14) == 0) {
                    uint32_t vIn, vOut, aIn, aOut;
                    SENSORS_GetRawSums(&vIn, &vOut, &aIn, &aOut);
                    cal->vInRawLow = (uint16_t)(vIn / (ADC_SAMPLE_COUNT / 2));
                    cal->vOutRawLow = (uint16_t)(vOut / (ADC_SAMPLE_COUNT / 2));
                    cal->vInRealLow_mV = cal->vOutRealLow_mV = (uint16_t)atoi(cmdBuffer + 14);
                    printf("ACK:CAL_V_LOW_OK:%d,%d\n", cal->vInRawLow, cal->vOutRawLow);
                } else if (strncmp(cmdBuffer, "CMD:CAL_V_HIGH:", 15) == 0) {
                    uint32_t vIn, vOut, aIn, aOut;
                    SENSORS_GetRawSums(&vIn, &vOut, &aIn, &aOut);
                    cal->vInRawHigh = (uint16_t)(vIn / (ADC_SAMPLE_COUNT / 2));
                    cal->vOutRawHigh = (uint16_t)(vOut / (ADC_SAMPLE_COUNT / 2));
                    cal->vInRealHigh_mV = cal->vOutRealHigh_mV = (uint16_t)atoi(cmdBuffer + 15);
                    printf("ACK:CAL_V_HIGH_OK:%d,%d\n", cal->vInRawHigh, cal->vOutRawHigh);
                } else if (strcmp(cmdBuffer, "CMD:CAL_SAVE") == 0) {
                    SETTINGS_SaveCalibration();
                    printf("ACK:CAL_SAVE_OK\n");
                } else if (strncmp(cmdBuffer, "CMD:SET_V_MAX:", 14) == 0) {
                    limits->batteryMax_mV = atoi(cmdBuffer + 14);
                    printf("ACK:SET_V_MAX_OK:%ld\n", limits->batteryMax_mV);
                } else if (strncmp(cmdBuffer, "CMD:SET_V_MIN:", 14) == 0) {
                    limits->batteryMin_mV = atoi(cmdBuffer + 14);
                    printf("ACK:SET_V_MIN_OK:%ld\n", limits->batteryMin_mV);
                } else if (strncmp(cmdBuffer, "CMD:SET_I_MAX:", 14) == 0) {
                    limits->chargingCurrent_mA = atoi(cmdBuffer + 14);
                    printf("ACK:SET_I_MAX_OK:%ld\n", limits->chargingCurrent_mA);
                } else if (strncmp(cmdBuffer, "CMD:SET_IN_V_MAX:", 17) == 0) {
                    limits->inputVoltageMax_mV = atoi(cmdBuffer + 17);
                    printf("ACK:SET_IN_V_MAX_OK:%ld\n", limits->inputVoltageMax_mV);
                } else if (strncmp(cmdBuffer, "CMD:SET_IN_I_MAX:", 17) == 0) {
                    limits->inputCurrentMax_mA = atoi(cmdBuffer + 17);
                    printf("ACK:SET_IN_I_MAX_OK:%ld\n", limits->inputCurrentMax_mA);
                } else if (strcmp(cmdBuffer, "CMD:RESET_FAULT") == 0) {
                    CONTROLLER_ResetFault();
                    printf("ACK:RESET_FAULT_OK\n");
                } else if (strcmp(cmdBuffer, "CMD:LIMITS_SAVE") == 0) {
                    SETTINGS_SaveLimits();
                    printf("ACK:LIMITS_SAVE_OK\n");
                } else if (strcmp(cmdBuffer, "CMD:HELP") == 0) {
                    printf("Commands: CAL_ENTER, CAL_EXIT, CAL_MODE_I, CAL_MODE_V, CAL_I_LOW:<mA>, CAL_I_HIGH:<mA>, CAL_V_LOW:<mV>, CAL_V_HIGH:<mV>, CAL_SAVE, SET_V_MAX:<mV>, SET_V_MIN:<mV>, SET_I_MAX:<mA>, SET_IN_V_MAX:<mV>, SET_IN_I_MAX:<mA>, LIMITS_SAVE, RESET_FAULT\n");
                }
                
                cmdIdx = 0;
            }
        } else if (cmdIdx < 63) {
            cmdBuffer[cmdIdx++] = c;
        }
    }
}
