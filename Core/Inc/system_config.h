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

// --- Hardware Constants ---
#define DITHER_TABLE_SIZE     8
#define ADC_CHANNEL_COUNT     6
#define ADC_SAMPLE_COUNT      128
#define ADC_BUF_LEN           (ADC_CHANNEL_COUNT * ADC_SAMPLE_COUNT)

// --- Algorithm Constants ---
#define MIN_VOLTAGE_IN_MV     10000
#define POWER_THRESHOLD_UW    50000
#define MPPT_STEP_SIZE_TICKS  8
#define SWEEP_STEP_SIZE_TICKS 19
#define MIN_INPUT_VOLTAGE_MPPT_MV 14000

// --- Sensor Physics ---
#define V_REF_INT_X1000       1200 // 1.2V * 1000
#define AVG_SLOPE_X1000       4300 // 4.3mV/C * 1000
#define V30_MV                1430 // 1.43V * 1000

#define constrain(amt, low, high) ((amt) < (low) ? (low) : ((amt) > (high) ? (high) : (amt)))

#endif /* __SYSTEM_CONFIG_H */
