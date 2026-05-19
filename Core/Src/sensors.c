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

/* Private constants */
#define FILTER_SHIFT 10  // 10 bits of fractional precision for EMA

/* Private variables (Encapsulated) */
static volatile uint16_t adc_buf[ADC_BUF_LEN];
static volatile uint8_t bufferFull = 0;
static Measurements_t measurements;

/* Filter states for software low-pass filtering (EMA) on RAW ADC values */
/* These are scaled by (1 << FILTER_SHIFT) */
static int32_t f_vIn_raw_fp = 0;
static int32_t f_vOut_raw_fp = 0;
static int32_t f_aIn_raw_fp = 0;
static int32_t f_aOut_raw_fp = 0;

/* Slow filters for Dashboard/Calibration UI (higher stability, more lag) */
static int32_t f_vIn_slow_fp = 0;
static int32_t f_vOut_slow_fp = 0;
static int32_t f_aIn_slow_fp = 0;
static int32_t f_aOut_slow_fp = 0;

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
    // Returns the SLOW filtered sum of 64 samples for UI stability
    *vIn = (uint32_t)(f_vIn_slow_fp >> FILTER_SHIFT);
    *vOut = (uint32_t)(f_vOut_slow_fp >> FILTER_SHIFT);
    *aIn = (uint32_t)(f_aIn_slow_fp >> FILTER_SHIFT);
    *aOut = (uint32_t)(f_aOut_slow_fp >> FILTER_SHIFT);
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

    // 1. Initialize filters on first run to avoid slow ramp-up
    if (f_vIn_raw_fp == 0 && voltIn_sum > 0) {
        f_vIn_raw_fp = f_vIn_slow_fp = (int32_t)voltIn_sum << FILTER_SHIFT;
        f_vOut_raw_fp = f_vOut_slow_fp = (int32_t)voltOut_sum << FILTER_SHIFT;
        f_aIn_raw_fp = f_aIn_slow_fp = (int32_t)ampIn_sum << FILTER_SHIFT;
        f_aOut_raw_fp = f_aOut_slow_fp = (int32_t)ampOut_sum << FILTER_SHIFT;
    }

    // 2. Apply High-Precision EMA filtering
    
    // Fast filter: Alpha = 0.25 (shift 2) for MPPT/Control Loop (Balanced lag/stability)
    f_vIn_raw_fp += (((int32_t)voltIn_sum << FILTER_SHIFT) - f_vIn_raw_fp) >> 2;
    f_vOut_raw_fp += (((int32_t)voltOut_sum << FILTER_SHIFT) - f_vOut_raw_fp) >> 2;
    f_aIn_raw_fp += (((int32_t)ampIn_sum << FILTER_SHIFT) - f_aIn_raw_fp) >> 2;
    f_aOut_raw_fp += (((int32_t)ampOut_sum << FILTER_SHIFT) - f_aOut_raw_fp) >> 2;

    // Slow filter: Alpha = 0.0625 (shift 4) for Dashboard/Calibration (high stability)
    f_vIn_slow_fp += (((int32_t)voltIn_sum << FILTER_SHIFT) - f_vIn_slow_fp) >> 4;
    f_vOut_slow_fp += (((int32_t)voltOut_sum << FILTER_SHIFT) - f_vOut_slow_fp) >> 4;
    f_aIn_slow_fp += (((int32_t)ampIn_sum << FILTER_SHIFT) - f_aIn_slow_fp) >> 4;
    f_aOut_slow_fp += (((int32_t)ampOut_sum << FILTER_SHIFT) - f_aOut_slow_fp) >> 4;

    // 3. Extract high-precision average from FAST filter for control loop
    int64_t v_in_avg_x1000 = ((int64_t)f_vIn_raw_fp * 1000) / (samples_per_half << FILTER_SHIFT);
    int64_t v_out_avg_x1000 = ((int64_t)f_vOut_raw_fp * 1000) / (samples_per_half << FILTER_SHIFT);
    int64_t a_in_avg_x1000 = ((int64_t)f_aIn_raw_fp * 1000) / (samples_per_half << FILTER_SHIFT);
    int64_t a_out_avg_x1000 = ((int64_t)f_aOut_raw_fp * 1000) / (samples_per_half << FILTER_SHIFT);

    // 4. Scaling to SI units (mV, mA) using the high-precision filtered values
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

    // 5. Temperature and Power (Non-filtered raw sums are fine for temperature)
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
