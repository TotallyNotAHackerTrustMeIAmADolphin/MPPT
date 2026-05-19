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
#define MPPT_INTERVAL_MS      25  
#define SWEEP_INTERVAL_SECONDS 300

// --- Hysteresis and Thresholds ---
#define HYSTERESIS_VOLTAGE_MV 200  // 200mV drop before leaving CV
#define HYSTERESIS_CURRENT_MA 100  // 100mA drop before leaving CC

// --- Hardware Constants ---
#define DITHER_TABLE_SIZE     8
#define ADC_CHANNEL_COUNT     6
#define ADC_SAMPLE_COUNT      128
#define ADC_BUF_LEN           (ADC_CHANNEL_COUNT * ADC_SAMPLE_COUNT)

// --- Algorithm Constants ---
#define MIN_VOLTAGE_IN_MV     14000  // Start voltage required
#define POWER_THRESHOLD_UW    10000  // 10mW threshold for direction change
#define SWEEP_STEP_SIZE_TICKS 8
#define MIN_INPUT_VOLTAGE_MPPT_MV 14000

// --- VSS Adaptive P&O Constants ---
#define VSS_N_FACTOR          2      // Balance speed and stability
#define VSS_MIN_STEP          1      // 1 tick micro-stepping
#define VSS_MAX_STEP          15     // Max jump limited to 15 ticks
#define VSS_VOLTAGE_DEADBAND  20     // 20mV deadband

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
