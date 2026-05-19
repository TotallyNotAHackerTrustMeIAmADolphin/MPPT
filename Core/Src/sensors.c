/**
  ******************************************************************************
  * @file           : sensors.c
  * @brief          : Sensor data acquisition and processing implementation.
  ******************************************************************************
  */

#include "sensors.h"
#include "system_config.h"
#include "settings.h"
#include "adc.h"
#include "main.h"
#include <string.h>

/* Private variables (Encapsulated) */
static volatile uint16_t adc_buf[ADC_BUF_LEN];
static volatile uint8_t bufferFull = 0;
static Measurements_t measurements;
static uint32_t vInRawSum, vOutRawSum, aInRawSum, aOutRawSum;

/* Filter states for software low-pass filtering (EMA) */
static int32_t f_vIn_mV = 0;
static int32_t f_vOut_mV = 0;
static int32_t f_aIn_mA = 0;
static int32_t f_aOut_mA = 0;

/* Filter states for RAW ADC values (used during calibration) */
static int32_t f_vIn_raw = 0;
static int32_t f_vOut_raw = 0;
static int32_t f_aIn_raw = 0;
static int32_t f_aOut_raw = 0;

void SENSORS_Init(void) {
    memset(&measurements, 0, sizeof(Measurements_t));
    HAL_ADC_Start_DMA(&hadc, (uint32_t *)adc_buf, ADC_BUF_LEN);
}

uint8_t SENSORS_IsBufferReady(void) {
    return bufferFull;
}

void SENSORS_ClearBufferReady(void) {
    bufferFull = 0;
}

const Measurements_t* SENSORS_GetMeasurements(void) {
    return &measurements;
}

void SENSORS_GetRawSums(uint32_t *vIn, uint32_t *vOut, uint32_t *aIn, uint32_t *aOut) {
    *vIn = (uint32_t)(f_vIn_raw / (ADC_SAMPLE_COUNT / 2));
    *vOut = (uint32_t)(f_vOut_raw / (ADC_SAMPLE_COUNT / 2));
    *aIn = (uint32_t)(f_aIn_raw / (ADC_SAMPLE_COUNT / 2));
    *aOut = (uint32_t)(f_aOut_raw / (ADC_SAMPLE_COUNT / 2));
}

void SENSORS_Process(uint16_t offset) {
    const Calibration_t *cal = SETTINGS_GetCalibration();
    uint32_t voltIn_temp = 0, voltOut_temp = 0;
    int32_t AmpIn_temp = 0, AmpOut_temp = 0;
    uint32_t tempMofets_temp = 0, tempMCU_temp = 0;

    uint16_t end = offset + (ADC_BUF_LEN / 2);
    
    uint32_t vInSum = 0, vOutSum = 0, aInSum = 0, aOutSum = 0;

    for (int i = offset; i < end; i += ADC_CHANNEL_COUNT) {
        voltIn_temp += adc_buf[i];
        AmpIn_temp += adc_buf[i + 1];
        voltOut_temp += adc_buf[i + 2];
        AmpOut_temp += adc_buf[i + 3];
        tempMofets_temp += adc_buf[i + 4];
        tempMCU_temp += adc_buf[i + 5];

        vInSum += adc_buf[i];
        aInSum += adc_buf[i + 1];
        vOutSum += adc_buf[i + 2];
        aOutSum += adc_buf[i + 3];
    }

    const int32_t samples_per_half = ADC_SAMPLE_COUNT / 2;

    // Apply EMA filtering to RAW sums (scaled up by samples_per_half to maintain precision)
    // Using alpha=0.125 (shift 3) for smooth calibration values
    f_vIn_raw += ((int32_t)vInSum - (f_vIn_raw / samples_per_half)) >> 3;
    f_vOut_raw += ((int32_t)vOutSum - (f_vOut_raw / samples_per_half)) >> 3;
    f_aIn_raw += ((int32_t)aInSum - (f_aIn_raw / samples_per_half)) >> 3;
    f_aOut_raw += ((int32_t)aOutSum - (f_aOut_raw / samples_per_half)) >> 3;

    // Update the raw sums used for GetRawSums
    vInRawSum = (uint32_t)(f_vIn_raw / samples_per_half);
    vOutRawSum = (uint32_t)(f_vOut_raw / samples_per_half);
    aInRawSum = (uint32_t)(f_aIn_raw / samples_per_half);
    aOutRawSum = (uint32_t)(f_aOut_raw / samples_per_half);

    // Scaling for Voltages (mV)
    int64_t v_in_avg_x1000 = ((int64_t)voltIn_temp * 1000) / samples_per_half;
    int32_t raw_vIn_mV = (int32_t)((v_in_avg_x1000 - (int64_t)cal->vInRawLow * 1000) * 
        ((int32_t)cal->vInRealHigh_mV - (int32_t)cal->vInRealLow_mV) / 
        ((int32_t)cal->vInRawHigh - (int32_t)cal->vInRawLow) / 1000 + cal->vInRealLow_mV);

    int64_t v_out_avg_x1000 = ((int64_t)voltOut_temp * 1000) / samples_per_half;
    int32_t raw_vOut_mV = (int32_t)((v_out_avg_x1000 - (int64_t)cal->vOutRawLow * 1000) * 
        ((int32_t)cal->vOutRealHigh_mV - (int32_t)cal->vOutRealLow_mV) / 
        ((int32_t)cal->vOutRawHigh - (int32_t)cal->vOutRawLow) / 1000 + cal->vOutRealLow_mV);

    // Scaling for Currents (mA)
    int64_t a_in_avg_x1000 = ((int64_t)AmpIn_temp * 1000) / samples_per_half;
    int32_t raw_aIn_mA = (int32_t)((a_in_avg_x1000 - (int64_t)cal->aInRawLow * 1000) * 
        ((int32_t)cal->aInRealHigh_mA - (int32_t)cal->aInRealLow_mA) / 
        ((int32_t)cal->aInRawHigh - (int32_t)cal->aInRawLow) / 1000 + cal->aInRealLow_mA);

    int64_t a_out_avg_x1000 = ((int64_t)AmpOut_temp * 1000) / samples_per_half;
    int32_t raw_aOut_mA = (int32_t)((a_out_avg_x1000 - (int64_t)cal->aOutRawLow * 1000) * 
        ((int32_t)cal->aOutRealHigh_mA - (int32_t)cal->aOutRealLow_mA) / 
        ((int32_t)cal->aOutRawHigh - (int32_t)cal->aOutRawLow) / 1000 + cal->aOutRealLow_mA);

    // Apply EMA Filtering (Software Low-Pass Filter)
    // Shift of 2 for voltage (alpha=0.25), shift of 3 for current (alpha=0.125)
    f_vIn_mV += (raw_vIn_mV - f_vIn_mV) >> 2;
    f_vOut_mV += (raw_vOut_mV - f_vOut_mV) >> 2;
    f_aIn_mA += (raw_aIn_mA - f_aIn_mA) >> 3;
    f_aOut_mA += (raw_aOut_mA - f_aOut_mA) >> 3;

    measurements.voltageIn_mV = f_vIn_mV;
    measurements.voltageOut_mV = f_vOut_mV;
    measurements.currentIn_mA = f_aIn_mA;
    measurements.currentOut_mA = f_aOut_mA;

    measurements.tempMosfets_C_x100 = (int32_t)((int64_t)tempMofets_temp * 100 / samples_per_half);
    
    int32_t mcu_adc_avg = tempMCU_temp / samples_per_half;
    int32_t mcu_v_sense_mv = (mcu_adc_avg * 3300) / 4096;
    measurements.tempMCU_C_x100 = (int32_t)((((int64_t)V30_MV - mcu_v_sense_mv) * 1000) / 43 + 3000);

    // Power in uW and mW
    measurements.powerIn_uW = (int64_t)measurements.voltageIn_mV * measurements.currentIn_mA;
    measurements.powerOut_uW = (int64_t)measurements.voltageOut_mV * measurements.currentOut_mA;
    measurements.powerIn_mW = (int32_t)(measurements.powerIn_uW / 1000);
    measurements.powerOut_mW = (int32_t)(measurements.powerOut_uW / 1000);

    // Efficiency
    if (measurements.powerIn_mW > 10) {
        measurements.efficiency_x100 = (uint16_t)((measurements.powerOut_uW * 10000) / measurements.powerIn_uW);
    } else {
        measurements.efficiency_x100 = 0;
    }
}

/* ADC Callbacks */
void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc) {
    bufferFull = 1;
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc) {
    bufferFull = 2;
}
