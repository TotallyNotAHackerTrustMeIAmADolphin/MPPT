/**
  ******************************************************************************
  * @file           : system_types.h
  * @brief          : Shared data structures and types for the MPPT system.
  ******************************************************************************
  */

#ifndef __SYSTEM_TYPES_H
#define __SYSTEM_TYPES_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief System operational states
 */
typedef enum
{
  STATE_IDLE,      // System waiting for conditions (e.g. low input voltage)
  STATE_SWEEPING,  // Performing a global MPPT sweep
  STATE_ACTIVE,    // Unified control mode (MPPT/CV/CC/Reverse/PSU)
  STATE_FAULT,     // Critical error (over-voltage/current/temp)
  STATE_RECOVERY   // Waiting after a fault
} SystemState_t;

/**
 * @brief System operation modes
 */
typedef enum
{
  MODE_MPPT,
  MODE_BIDIRECTIONAL,
  MODE_POWER_SUPPLY
} OperationMode_t;

/**
 * @brief Specific reasons for a system fault
 */
typedef enum
{
  FAULT_REASON_NONE,
  FAULT_REASON_INPUT_OV,    // Input Over-voltage
  FAULT_REASON_INPUT_UV,    // Input Under-voltage
  FAULT_REASON_INPUT_OC,    // Input Over-current
  FAULT_REASON_OUTPUT_OV,   // Output Over-voltage
  FAULT_REASON_OUTPUT_OC,   // Output Over-current (Hard Hardware Limit)
  FAULT_REASON_BACKFLOW,    // Reverse current detected
  FAULT_REASON_OVERTEMP     // Overtemperature detected
} FaultReason_t;

/**
 * @brief Active regulation (soft) limits
 */
typedef enum
{
  LIMIT_NONE,       // No limit active (MPPT tracking)
  LIMIT_V_OUT_MAX,  // Output Voltage Max (Forward CV)
  LIMIT_I_OUT_MAX,  // Output Current Max (Forward CC)
  LIMIT_V_IN_MIN,   // Input Voltage Min (Brownout regulation)
  LIMIT_V_IN_MAX,   // Input Voltage Max (Reverse Flow CV)
  LIMIT_I_OUT_MIN,  // Output Current Min (Reverse Flow CC or Backflow)
  LIMIT_SWEEPING    // Sweep in progress
} SoftLimit_t;

/**
 * @brief PID Controller state and configuration
 */
typedef struct
{
  int32_t Kp;
  int32_t Ki;
  int32_t Kd;
  int32_t previousError;
  int32_t previousInput;
  int64_t integral;
  int32_t setPoint;
  int32_t *input;      // Pointer to the measured value (e.g. voltageOut_mV)
  int32_t *output;     // Pointer to the control target (e.g. dutyCycle_ticks)
  int64_t maxIntegral;
  int32_t minOutput;
  int32_t maxOutput;
} PID_t;

/**
 * @brief Sensor measurement data in physical units
 */
typedef struct
{
  int32_t voltageIn_mV;
  int32_t voltageOut_mV;
  int32_t currentIn_mA;
  int32_t currentOut_mA;
  int32_t powerIn_mW;
  int32_t powerOut_mW;
  int64_t powerIn_uW;
  int64_t powerOut_uW;
  int32_t tempMCU_C_x100;
  int32_t tempMosfets_C_x100;
  uint16_t efficiency_x100;
} Measurements_t;

/**
 * @brief System operational limits
 */
typedef struct
{
  OperationMode_t mode;
  int32_t vOutMax_mV;
  int32_t iOutMax_mA;
  int32_t vInMin_mV;
  int32_t vInMax_mV;
  int32_t iOutMin_mA;  // Typically negative for reverse flow
} DeviceLimits_t;

/**
 * @brief Sensor calibration constants
 */
typedef struct
{
  uint16_t vInRawLow;
  uint16_t vInRealLow_mV;
  uint16_t vInRawHigh;
  uint16_t vInRealHigh_mV;

  uint16_t vOutRawLow;
  uint16_t vOutRealLow_mV;
  uint16_t vOutRawHigh;
  uint16_t vOutRealHigh_mV;

  uint16_t aInRawLow;
  uint16_t aInRealLow_mA;
  uint16_t aInRawHigh;
  uint16_t aInRealHigh_mA;

  uint16_t aOutRawLow;
  uint16_t aOutRealLow_mA;
  uint16_t aOutRawHigh;
  uint16_t aOutRealHigh_mA;
} Calibration_t;

#endif /* __SYSTEM_TYPES_H */
