/**
  ******************************************************************************
  * @file           : system_config.h
  * @brief          : Global configuration constants for the MPPT system.
  ******************************************************************************
  */

#ifndef __SYSTEM_CONFIG_H
#define __SYSTEM_CONFIG_H

// --- Timing Configurations ---
#define TELEMETRY_INTERVAL_MS 100 
#define MPPT_INTERVAL_MS      40  
#define SWEEP_INTERVAL_SECONDS 300

// --- Hysteresis and Thresholds ---
#define HYSTERESIS_VOLTAGE_MV 500  // 500mV drop before leaving CV
#define HYSTERESIS_CURRENT_MA 200  // 200mA drop before leaving CC

// --- Hardware Constants ---
#define DITHER_TABLE_SIZE     8
#define ADC_CHANNEL_COUNT     6
#define ADC_SAMPLE_COUNT      128
#define ADC_BUF_LEN           (ADC_CHANNEL_COUNT * ADC_SAMPLE_COUNT)

// --- Algorithm Constants ---
#define MIN_VOLTAGE_IN_MV     14000  // Start voltage required
#define POWER_THRESHOLD_UW    50000
#define MPPT_STEP_SIZE_TICKS  8
#define SWEEP_STEP_SIZE_TICKS 19
#define MIN_INPUT_VOLTAGE_MPPT_MV 14000

// --- Sensor Physics ---
#define V_REF_INT_X1000       1200 // 1.2V * 1000
#define AVG_SLOPE_X1000       4300 // 4.3mV/C * 1000
#define V30_MV                1430 // 1.43V * 1000

// --- Hardware Safety Limits ---
#define HARD_LIMIT_VIN_MAX_MV   80000  // 80V Max Input
#define HARD_LIMIT_VIN_MIN_MV   12500  // 12,5V Min Input (Protect Aux Supply)
#define HARD_LIMIT_VOUT_MAX_MV  80000  // 80V Max Output
#define HARD_LIMIT_IIN_MAX_MA   20000  // 20A Max Input Current
#define HARD_LIMIT_IOUT_MAX_MA  20000  // 20A Max Output Current
#define HARD_LIMIT_TEMP_MAX_C   85     // 85°C Max Internal Temp

#define constrain(amt, low, high) ((amt) < (low) ? (low) : ((amt) > (high) ? (high) : (amt)))

#endif /* __SYSTEM_CONFIG_H */
