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

/* Filter states for software low-pass filtering (EMA) on RAW ADC values */
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
    const int32_t samples_per_half = ADC_SAMPLE_COUNT / 2;
    *vIn = (uint32_t)(f_vIn_raw / samples_per_half);
    *vOut = (uint32_t)(f_vOut_raw / samples_per_half);
    *aIn = (uint32_t)(f_aIn_raw / samples_per_half);
    *aOut = (uint32_t)(f_aOut_raw / samples_per_half);
}

void SENSORS_Process(uint16_t offset) {
    const Calibration_t *cal = SETTINGS_GetCalibration();
    uint32_t voltIn_sum = 0, voltOut_sum = 0;
    uint32_t ampIn_sum = 0, ampOut_sum = 0;
    uint32_t tempMofets_sum = 0, tempMCU_sum = 0;

    uint16_t end = offset + (ADC_BUF_LEN / 2);
    
    for (int i = offset; i < end; i += ADC_CHANNEL_COUNT) {
        voltIn_sum += adc_buf[i];
        ampIn_sum += adc_buf[i + 1];
        voltOut_sum += adc_buf[i + 2];
        ampOut_sum += adc_buf[i + 3];
        tempMofets_sum += adc_buf[i + 4];
        tempMCU_sum += adc_buf[i + 5];
    }

    const int32_t samples_per_half = ADC_SAMPLE_COUNT / 2;

    // 1. Initialize filters on first run
    if (f_vIn_raw == 0 && voltIn_sum > 0) {
        f_vIn_raw = voltIn_sum * samples_per_half;
        f_vOut_raw = voltOut_sum * samples_per_half;
        f_aIn_raw = ampIn_sum * samples_per_half;
        f_aOut_raw = ampOut_sum * samples_per_half;
    }

    // 2. Apply EMA filtering to RAW sums (scaled up to maintain precision)
    // Shift of 3 (alpha = 0.125) for high stability
    f_vIn_raw += ((int32_t)voltIn_sum - (f_vIn_raw / samples_per_half)) >> 3;
    f_vOut_raw += ((int32_t)voltOut_sum - (f_vOut_raw / samples_per_half)) >> 3;
    f_aIn_raw += ((int32_t)ampIn_sum - (f_aIn_raw / samples_per_half)) >> 3;
    f_aOut_raw += ((int32_t)ampOut_sum - (f_aOut_raw / samples_per_half)) >> 3;

    // 3. Derived Filtered Values for internal use
    int64_t v_in_avg_x1000 = ((int64_t)f_vIn_raw * 1000) / (samples_per_half * samples_per_half);
    int64_t v_out_avg_x1000 = ((int64_t)f_vOut_raw * 1000) / (samples_per_half * samples_per_half);
    int64_t a_in_avg_x1000 = ((int64_t)f_aIn_raw * 1000) / (samples_per_half * samples_per_half);
    int64_t a_out_avg_x1000 = ((int64_t)f_aOut_raw * 1000) / (samples_per_half * samples_per_half);

    // 4. Scaling to SI units (mV, mA) using filtered values
    measurements.voltageIn_mV = (int32_t)((v_in_avg_x1000 - (int64_t)cal->vInRawLow * 1000) * 
        ((int32_t)cal->vInRealHigh_mV - (int32_t)cal->vInRealLow_mV) / 
        ((int32_t)cal->vInRawHigh - (int32_t)cal->vInRawLow) / 1000 + cal->vInRealLow_mV);

    measurements.voltageOut_mV = (int32_t)((v_out_avg_x1000 - (int64_t)cal->vOutRawLow * 1000) * 
        ((int32_t)cal->vOutRealHigh_mV - (int32_t)cal->vOutRealLow_mV) / 
        ((int32_t)cal->vOutRawHigh - (int32_t)cal->vOutRawLow) / 1000 + cal->vOutRealLow_mV);

    measurements.currentIn_mA = (int32_t)((a_in_avg_x1000 - (int64_t)cal->aInRawLow * 1000) * 
        ((int32_t)cal->aInRealHigh_mA - (int32_t)cal->aInRealLow_mA) / 
        ((int32_t)cal->aInRawHigh - (int32_t)cal->aInRawLow) / 1000 + cal->aInRealLow_mA);

    measurements.currentOut_mA = (int32_t)((a_out_avg_x1000 - (int64_t)cal->aOutRawLow * 1000) * 
        ((int32_t)cal->aOutRealHigh_mA - (int32_t)cal->aOutRealLow_mA) / 
        ((int32_t)cal->aOutRawHigh - (int32_t)cal->aOutRawLow) / 1000 + cal->aOutRealLow_mA);

    // Temperature and Power
    measurements.tempMosfets_C_x100 = (int32_t)((int64_t)tempMofets_sum * 100 / samples_per_half);
    
    int32_t mcu_adc_avg = tempMCU_sum / samples_per_half;
    int32_t mcu_v_sense_mv = (mcu_adc_avg * 3300) / 4096;
    measurements.tempMCU_C_x100 = (int32_t)((((int64_t)V30_MV - mcu_v_sense_mv) * 1000) / 43 + 3000);

    measurements.powerIn_uW = (int64_t)measurements.voltageIn_mV * measurements.currentIn_mA;
    measurements.powerOut_uW = (int64_t)measurements.voltageOut_mV * measurements.currentOut_mA;
    measurements.powerIn_mW = (int32_t)(measurements.powerIn_uW / 1000);
    measurements.powerOut_mW = (int32_t)(measurements.powerOut_uW / 1000);

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
