/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body (Refactored for Clarity and Robustness)
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2024 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "dma.h"
#include "i2c.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "usb_device.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "usbd_cdc_if.h"
#include "string.h"
#include <math.h>
#include <stdio.h>
#include <stdbool.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

// --- System State and Settings Structures ---

// Holds all real-time sensor data and status flags
typedef struct {
    float voltIn;
    float voltOut;
    float ampIn;
    float ampOut;
    float wattIn;
    float wattOut;
    float tempMCU;
    float tempMosfets;
    
    // Status Flags
    bool overCurrentIn;
    bool overCurrentOut;
    bool overVoltageIn;
    bool overVoltageOut;
    bool underVoltageIn;
    bool underVoltageOut;
} ControllerState_t;

// Holds all configurable system limits and parameters
typedef struct {
    float maxInputVoltage;
    float maxInputCurrent;
    float minInputVoltage;

    float batteryMaxVoltage;
    float batteryMinVoltage;
    float batteryChargeCurrent;
    
    // PWM settings
    float dutyCycleMax; // e.g., 170.0 for a boost range up to 70%
    float pwmDeadBand;
    
    // MPPT settings
    float mpptMinInputVoltage;
} SystemSettings_t;

// Defines the main operating modes of the controller
typedef enum {
    MODE_OFF,
    MODE_MPPT,
    MODE_CONSTANT_CURRENT,
    MODE_CONSTANT_VOLTAGE,
    MODE_FAULT
} OperatingMode_t;

// State machine for the global MPPT sweep
typedef enum {
    MPPT_STATE_TRACKING,
    MPPT_STATE_SWEEPING
} MpptSubState_t;

// PID Controller Structure
typedef struct {
  float Kp, Ki, Kd;
  float integral, previousError;
  float setPoint;
  float *input, *output;
  float minOutput, maxOutput;
  float maxIntegral;
} PID_t;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
// --- Hardware & System Defines ---
#define CALIBRATION_MODE      false   // Set true to print raw ADC values for calibration
#define DEBUG_PRINT_INTERVAL  64      // Print debug info every N main loops
#define ADC_UPDATE_DIVIDER    4       // Run control logic every N ADC updates

// --- PWM Configuration ---
#define PWM_TIMER_PERIOD      ((uint16_t)240) // Results in 100 kHz PWM frequency
#define DITHER_TABLE_SIZE     8               // 3 bits of dithering (must be power of 2)
#define DITHER_BITS           3               // log2(DITHER_TABLE_SIZE)

// --- ADC Configuration ---
#define ADC_CHANNEL_COUNT     6
#define ADC_CHANNEL_AVG_BITS  7
#define ADC_SAMPLE_COUNT      (1 << ADC_CHANNEL_AVG_BITS)
#define ADC_BUF_LEN           (ADC_CHANNEL_COUNT * ADC_SAMPLE_COUNT)

// --- Sensor Calibration Defines (Slopes and Offsets) ---
// V_actual = (ADC_avg - RAW_LOW) * (REF_HIGH - REF_LOW) / (RAW_HIGH - RAW_LOW) + REF_LOW
#define VOLT_IN_RAW_LOW       950.603760f
#define VOLT_IN_RAW_HIGH      2684.238525f
#define VOLT_OUT_RAW_LOW      891.904358f
#define VOLT_OUT_RAW_HIGH     2729.892090f
#define VOLT_REFERENCE_LOW    10.0f
#define VOLT_REFERENCE_HIGH   30.0f

#define AMP_IN_RAW_LOW        1987.640991f
#define AMP_IN_RAW_HIGH       1674.981445f
#define AMP_OUT_RAW_LOW       1986.062378f
#define AMP_OUT_RAW_HIGH      1660.860840f
#define AMP_REFERENCE_LOW     0.0f
#define AMP_REFERENCE_HIGH    3.9f

// Internal Temp Sensor
#define V_REF_INT             1.2f
#define AVG_SLOPE             (4.3f * 1000 * (4096 / V_REF_INT))
#define V30                   (1.43f * (4096 / V_REF_INT))

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
#define constrain(amt, low, high) ((amt) < (low) ? (low) : ((amt) > (high) ? (high) : (amt)))
#define map_float(x, in_min, in_max, out_min, out_max) (((x) - (in_min)) * ((out_max) - (out_min)) / ((in_max) - (in_min)) + (out_min))
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
// --- Global Controller Variables ---
const SystemSettings_t settings = {
    .maxInputVoltage = 80.0f,
    .maxInputCurrent = 20.0f,
    .minInputVoltage = 12.0f,
    .batteryMaxVoltage = 25.2f,
    .batteryMinVoltage = 18.0f,     // 6S Li-ion at 3.0V/cell
    .batteryChargeCurrent = 2.0f,
    .dutyCycleMax = 170.0f,
    .pwmDeadBand = 2.0f,
    .mpptMinInputVoltage = 15.0f    // Safety voltage for P&O algorithm
};

ControllerState_t state = {0};
OperatingMode_t currentMode = MODE_OFF;
float dutyCycle = 0.0f;
float minDutyCycle = 0.0f;

// --- Hardware-specific Variables ---
volatile bool adcBufferFull = false;
volatile uint16_t adc_buf[ADC_BUF_LEN];
uint16_t ditherTableCH1[DITHER_TABLE_SIZE];
uint16_t ditherTableCH2[DITHER_TABLE_SIZE];
const uint16_t PWM_MAX_DITHERED_VALUE = (PWM_TIMER_PERIOD * DITHER_TABLE_SIZE);

// --- PID Controller Instances ---
PID_t pid_CV = { .Kp = 0.1, .Ki = 0.0, .Kd = 0.0001, .maxIntegral = 1, .minOutput = 0, .maxOutput = settings.dutyCycleMax};
PID_t pid_CC = { .Kp = 0.1, .Ki = 0.0, .Kd = 0.0001, .maxIntegral = 1, .minOutput = 0, .maxOutput = settings.dutyCycleMax};

// --- MPPT Sweep State Variables ---
MpptSubState_t mpptSubState = MPPT_STATE_TRACKING;
const uint32_t SWEEP_INTERVAL_SECONDS = 300; // Sweep every 5 minutes
const float SWEEP_DUTY_STEP = 1.0f;
uint32_t sweepTriggerCounter = 0;
float sweepMaxPower = 0.0f;
float sweepBestDutyCycle = 0.0f;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
// Core Logic
void runStateMachine(void);
void updateStatusFlags(void);
void calculateMinDutyCycle(void);
void printDebugInfo(void);

// Hardware Control
void setPWM(float dutyCyclePct);
void updateDitherTable(uint16_t *pDitherTable, uint16_t desiredDutyCycle);
void readSensors(void);
void calibrateSensors(void);

// Control Algorithms
void computePID(PID_t *pid);
void runMpptAlgorithm(void);
void mpptPerturbAndObserve(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
// Empty
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* MCU Configuration--------------------------------------------------------*/
  HAL_Init();
  SystemClock_Config();

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_I2C2_Init();
  MX_TIM1_Init();
  MX_USART3_UART_Init();
  MX_USB_DEVICE_Init();
  MX_SPI1_Init();
  MX_TIM2_Init();
  MX_ADC_Init();
  MX_TIM3_Init();
  MX_TIM6_Init();
  
  /* USER CODE BEGIN 2 */
  // --- Initialize PID Controllers ---
  pid_CV.setPoint = settings.batteryMaxVoltage;
  pid_CV.input = &state.voltOut;
  pid_CV.output = &dutyCycle;
  
  pid_CC.setPoint = settings.batteryChargeCurrent;
  pid_CC.input = &state.ampOut;
  pid_CC.output = &dutyCycle;

  // --- Start PWM and ADC ---
  HAL_TIM_PWM_Start_DMA(&htim1, TIM_CHANNEL_1, (uint32_t *)ditherTableCH1, DITHER_TABLE_SIZE);
  HAL_TIM_PWM_Start_DMA(&htim1, TIM_CHANNEL_2, (uint32_t *)ditherTableCH2, DITHER_TABLE_SIZE);
  HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_1);
  HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_2);
  HAL_ADC_Start_DMA(&hadc, (uint32_t *)adc_buf, ADC_BUF_LEN);

  // --- Initial State ---
  setPWM(0);
  currentMode = MODE_OFF;
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    if (adcBufferFull)
    {
      static uint32_t loopCounter = 0;
      adcBufferFull = false;

      // 1. Acquire Data
      readSensors();

      if (CALIBRATION_MODE) {
          calibrateSensors();
          setPWM(100); // Set to passthrough for calibration
          continue; // Skip main logic
      }
      
      // 2. Run Control Logic at a slower rate than ADC updates
      if(loopCounter % ADC_UPDATE_DIVIDER == 0)
      {
          // 2a. Perform calculations and update status
          calculateMinDutyCycle();
          updateStatusFlags();

          // 2b. Run the core state machine to determine mode and duty cycle
          runStateMachine();

          // 2c. Final safety clamp on the duty cycle
          dutyCycle = constrain(dutyCycle, minDutyCycle, settings.dutyCycleMax);
          
          // 2d. Override duty cycle if system is OFF or in FAULT
          if(currentMode == MODE_OFF || currentMode == MODE_FAULT) {
              dutyCycle = 0;
          }

          // 2e. Set the physical PWM output
          setPWM(dutyCycle);
      }
      
      // 3. Optional Debugging Output
      if (loopCounter % DEBUG_PRINT_INTERVAL == 0) {
        printDebugInfo();
      }

      loopCounter++;
    }
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration (Auto-generated by CubeMX)
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI48;
  RCC_OscInitStruct.HSI48State = RCC_HSI48_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) Error_Handler();
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI48;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK) Error_Handler();
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USB;
  PeriphClkInit.UsbClockSelection = RCC_USBCLKSOURCE_HSI48;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK) Error_Handler();
}

/* USER CODE BEGIN 4 */

//==============================================================================
// Core Logic Functions
//==============================================================================

/**
 * @brief Main state machine, determines the operating mode based on system status.
 */
void runStateMachine(void)
{
    // --- State Transition Logic ---
    // Highest priority: Fault conditions
    if (state.overVoltageIn || state.overCurrentIn || state.underVoltageOut) {
        currentMode = MODE_FAULT;
    }
    // Next priority: System OFF condition (e.g. no sun)
    else if (state.underVoltageIn) {
        currentMode = MODE_OFF;
    }
    // Otherwise, determine charging mode
    else {
        if (state.overVoltageOut) {
            currentMode = MODE_CONSTANT_VOLTAGE;
        } else if (state.overCurrentOut) {
            currentMode = MODE_CONSTANT_CURRENT;
        } else {
            currentMode = MODE_MPPT;
        }
    }

    // --- State Action Logic ---
    switch (currentMode)
    {
        case MODE_MPPT:
            runMpptAlgorithm();
            break;

        case MODE_CONSTANT_CURRENT:
            computePID(&pid_CC);
            // Reset MPPT sweep when not in use
            mpptSubState = MPPT_STATE_TRACKING;
            sweepTriggerCounter = 0;
            break;

        case MODE_CONSTANT_VOLTAGE:
            computePID(&pid_CV);
            // Reset MPPT sweep when not in use
            mpptSubState = MPPT_STATE_TRACKING;
            sweepTriggerCounter = 0;
            break;

        case MODE_FAULT:
            // Actions for fault are handled by the main loop (PWM=0)
            // Could add fault-specific recovery logic here if needed
            break;
            
        case MODE_OFF:
            // Actions for OFF are handled by the main loop (PWM=0)
            // Reset trackers and counters
            mpptSubState = MPPT_STATE_TRACKING;
            sweepTriggerCounter = 0;
            pid_CC.integral = 0;
            pid_CV.integral = 0;
            break;
    }
}

/**
 * @brief Updates all boolean status flags in the ControllerState struct.
 */
void updateStatusFlags(void)
{
    state.underVoltageIn  = state.voltIn < settings.minInputVoltage;
    state.underVoltageOut = state.voltOut < settings.batteryMinVoltage;
    state.overVoltageIn   = state.voltIn > settings.maxInputVoltage;
    state.overVoltageOut  = state.voltOut > settings.batteryMaxVoltage;
    state.overCurrentIn   = state.ampIn > settings.maxInputCurrent;
    state.overCurrentOut  = state.ampOut > settings.batteryChargeCurrent;
}

/**
 * @brief Calculates the minimum duty cycle to prevent reverse current.
 * This creates a theoretical floor for the duty cycle.
 */
void calculateMinDutyCycle(void)
{
    if (state.voltIn <= 0) { // Avoid division by zero
        minDutyCycle = 0;
        return;
    }

    // Buck mode: D = Vout / Vin. Our scale is 0-100 for buck.
    if (state.voltOut < state.voltIn) {
        minDutyCycle = (state.voltOut / state.voltIn) * 100.0f;
    } 
    // Boost mode: D_boost = 1 - (Vin / Vout). Our scale is 100-200.
    // Duty = 100 + (1 - D_boost)*100 = 200 - (Vin/Vout)*100
    else {
        minDutyCycle = 200.0f - (state.voltIn / state.voltOut) * 100.0f;
    }
    // Ensure it's within a safe, practical range
    minDutyCycle = constrain(minDutyCycle, 0, settings.dutyCycleMax);
}

//==============================================================================
// Control Algorithm Functions
//==============================================================================

/**
 * @brief Generic PID controller computation.
 */
void computePID(PID_t *pid)
{
  float error = pid->setPoint - *pid->input;
  pid->integral += error;
  pid->integral = constrain(pid->integral, -pid->maxIntegral, pid->maxIntegral);

  float derivative = error - pid->previousError;

  *pid->output += pid->Kp * error + pid->Ki * pid->integral + pid->Kd * derivative;
  
  pid->previousError = error;
}

/**
 * @brief State machine for MPPT, handling both P&O tracking and periodic sweeps.
 */
void runMpptAlgorithm(void)
{
    // --- Sweep Trigger Logic ---
    if (mpptSubState == MPPT_STATE_TRACKING) {
        sweepTriggerCounter++;
        
        // Assuming this function is called at a fixed rate, calculate trigger point
        const uint32_t calls_per_second = (1000000 / 500) / ADC_UPDATE_DIVIDER; // Approx based on timers
        if (sweepTriggerCounter > (SWEEP_INTERVAL_SECONDS * calls_per_second))
        {
            mpptSubState = MPPT_STATE_SWEEPING;
            sweepTriggerCounter = 0;
            sweepMaxPower = 0.0f;
            dutyCycle = minDutyCycle; // Start sweep from the bottom
            sweepBestDutyCycle = dutyCycle;
        }
    }

    // --- Sub-State Action Logic ---
    switch (mpptSubState)
    {
        case MPPT_STATE_TRACKING:
            mpptPerturbAndObserve();
            break;

        case MPPT_STATE_SWEEPING:
            // Check if power at the current sweep step is a new maximum
            if (state.wattIn > sweepMaxPower) {
                sweepMaxPower = state.wattIn;
                sweepBestDutyCycle = dutyCycle;
            }

            // Increment duty cycle for the next step
            dutyCycle += SWEEP_DUTY_STEP;

            // Check if sweep is finished
            if (dutyCycle > settings.dutyCycleMax) {
                dutyCycle = sweepBestDutyCycle; // Jump to the best point found
                mpptSubState = MPPT_STATE_TRACKING; // Return to normal tracking
            }
            break;
    }
}

/**
 * @brief Core Perturb and Observe (P&O) hill-climbing algorithm.
 */
void mpptPerturbAndObserve(void)
{
    static float previousWattIn = 0.0f;
    static bool direction = true; // true = increase duty, false = decrease

    // Safety check to prevent panel voltage collapse
    if (state.voltIn < settings.mpptMinInputVoltage) {
        dutyCycle -= 1.0f; // Force a rapid reduction in load
        previousWattIn = state.wattIn; // Reset tracker
        return;
    }

    const float POWER_CHANGE_THRESHOLD = 0.05f; // 50mW
    float powerChange = state.wattIn - previousWattIn;

    if (fabs(powerChange) > POWER_CHANGE_THRESHOLD) {
        if (powerChange < 0) {
            direction = !direction; // Power dropped, reverse direction
        }
    }

    const float PO_STEP_SIZE = 0.3f;
    if (direction) {
        dutyCycle += PO_STEP_SIZE;
    } else {
        dutyCycle -= PO_STEP_SIZE;
    }
    
    previousWattIn = state.wattIn;
}


//==============================================================================
// Hardware Abstraction & Driver Functions
//==============================================================================

/**
 * @brief Sets the PWM duty cycles for the buck-boost converter.
 * @param dutyCyclePct A value from 0-200.
 *        0-100: Buck mode (duty applied to CH2)
 *        100: Passthrough
 *        100-200: Boost mode (duty applied to CH1)
 */
void setPWM(float dutyCyclePct)
{
    uint16_t ch1_val, ch2_val;

    // Apply deadband near 0 and max
    if (dutyCyclePct < settings.pwmDeadBand) dutyCyclePct = 0;
    if (dutyCyclePct > settings.dutyCycleMax - settings.pwmDeadBand) dutyCyclePct = settings.dutyCycleMax;

    // Apply deadband around the 100% passthrough point
    if (fabs(dutyCyclePct - 100.0f) < settings.pwmDeadBand) {
        dutyCyclePct = 100.0f;
    }

    if (dutyCyclePct <= 100.0f) // Buck or Passthrough Mode
    {
        ch1_val = PWM_MAX_DITHERED_VALUE; // Boost high-side MOSFET is fully ON
        ch2_val = (uint16_t)roundf( (dutyCyclePct / 100.0f) * PWM_MAX_DITHERED_VALUE );
    }
    else // Boost Mode
    {
        // Duty cycle is inverted for boost high-side MOSFET
        float boost_duty = 200.0f - dutyCyclePct;
        ch1_val = (uint16_t)roundf( (boost_duty / 100.0f) * PWM_MAX_DITHERED_VALUE );
        ch2_val = PWM_MAX_DITHERED_VALUE; // Buck MOSFET is fully ON
    }

    updateDitherTable(ditherTableCH1, ch1_val);
    updateDitherTable(ditherTableCH2, ch2_val);
}

/**
 * @brief Updates a dither table to achieve higher PWM resolution.
 */
void updateDitherTable(uint16_t *pDitherTable, uint16_t desiredDutyCycle)
{
    desiredDutyCycle = constrain(desiredDutyCycle, 0, PWM_MAX_DITHERED_VALUE);
    
    uint16_t baseDutyCycle = desiredDutyCycle >> DITHER_BITS;
    uint16_t ditherIndex = desiredDutyCycle & (DITHER_TABLE_SIZE - 1);

    for (uint16_t i = 0; i < DITHER_TABLE_SIZE; i++) {
        pDitherTable[i] = (i < ditherIndex) ? (baseDutyCycle + 1) : baseDutyCycle;
    }
}

/**
 * @brief Reads all sensor values from the ADC buffer and populates the global state struct.
 */
void readSensors(void)
{
    uint32_t voltIn_raw = 0, voltOut_raw = 0, ampIn_raw = 0, ampOut_raw = 0, tempMcu_raw = 0;

    for (int i = 0; i < ADC_BUF_LEN; i += ADC_CHANNEL_COUNT) {
        voltIn_raw  += adc_buf[i];
        ampIn_raw   += adc_buf[i + 1];
        voltOut_raw += adc_buf[i + 2];
        ampOut_raw  += adc_buf[i + 3];
        // tempMosfets_raw += adc_buf[i + 4]; // Placeholder
        tempMcu_raw += adc_buf[i + 5];
    }
    
    float v_in_avg  = (float)voltIn_raw  / ADC_SAMPLE_COUNT;
    float v_out_avg = (float)voltOut_raw / ADC_SAMPLE_COUNT;
    float a_in_avg  = (float)ampIn_raw   / ADC_SAMPLE_COUNT;
    float a_out_avg = (float)ampOut_raw  / ADC_SAMPLE_COUNT;
    float t_mcu_avg = (float)tempMcu_raw / ADC_SAMPLE_COUNT;

    state.voltIn = map_float(v_in_avg, VOLT_IN_RAW_LOW, VOLT_IN_RAW_HIGH, VOLT_REFERENCE_LOW, VOLT_REFERENCE_HIGH);
    state.voltOut= map_float(v_out_avg, VOLT_OUT_RAW_LOW, VOLT_OUT_RAW_HIGH, VOLT_REFERENCE_LOW, VOLT_REFERENCE_HIGH);
    state.ampIn  = map_float(a_in_avg, AMP_IN_RAW_LOW, AMP_IN_RAW_HIGH, AMP_REFERENCE_LOW, AMP_REFERENCE_HIGH);
    state.ampOut = map_float(a_out_avg, AMP_OUT_RAW_LOW, AMP_OUT_RAW_HIGH, AMP_REFERENCE_LOW, AMP_REFERENCE_HIGH);
    
    // Clamp negative currents to zero as they are often noise around the zero-point
    state.ampIn  = (state.ampIn < 0) ? 0.0f : state.ampIn;
    state.ampOut = (state.ampOut < 0) ? 0.0f : state.ampOut;
    
    state.wattIn = state.voltIn * state.ampIn;
    state.wattOut = state.voltOut * state.ampOut;
    
    state.tempMCU = (V30 - t_mcu_avg) / AVG_SLOPE + 30.0f;
}

/**
 * @brief Prints raw, averaged ADC values for calibration purposes.
 */
void calibrateSensors(void)
{
    // Uses a simple IIR filter to get a very stable reading for calibration
    static float v_in_filt=0, v_out_filt=0, a_in_filt=0, a_out_filt=0;
    const float alpha = 0.001f;
    
    uint32_t v_in_raw=0, v_out_raw=0, a_in_raw=0, a_out_raw=0;
    for (int i = 0; i < ADC_BUF_LEN; i += ADC_CHANNEL_COUNT) {
        v_in_raw += adc_buf[i]; a_in_raw += adc_buf[i+1];
        v_out_raw += adc_buf[i+2]; a_out_raw += adc_buf[i+3];
    }
    
    v_in_filt = v_in_filt*(1-alpha) + ((float)v_in_raw/ADC_SAMPLE_COUNT)*alpha;
    v_out_filt= v_out_filt*(1-alpha) + ((float)v_out_raw/ADC_SAMPLE_COUNT)*alpha;
    a_in_filt = a_in_filt*(1-alpha) + ((float)a_in_raw/ADC_SAMPLE_COUNT)*alpha;
    a_out_filt= a_out_filt*(1-alpha) + ((float)a_out_raw/ADC_SAMPLE_COUNT)*alpha;
    
    static uint32_t cal_counter = 0;
    if(cal_counter++ % 256 == 0) {
      printf("CALIBRATION -- Vin: %.2f\tVout: %.2f\tAin: %.2f\tAout: %.2f\n", v_in_filt, v_out_filt, a_in_filt, a_out_filt);
    }
}


//==============================================================================
// System & Utility Functions
//==============================================================================
/**
 * @brief Called by HAL when ADC buffer is half full.
 */
void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc) {
  adcBufferFull = true;
}
/**
 * @brief Called by HAL when ADC buffer is full. Also flag for processing.
 */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc) {
  // Can be used as well, but HalfCplt gives lower latency.
  // adcBufferFull = true; 
}

/**
 * @brief Retargets printf to USB CDC (Virtual COM Port).
 */
int _write(int file, char *ptr, int len)
{
  static uint8_t rc = USBD_OK;
  do {
    rc = CDC_Transmit_FS((uint8_t *)ptr, len);
  } while (USBD_BUSY == rc);
  return (USBD_FAIL == rc) ? 0 : len;
}

/**
 * @brief Prints system status information over serial.
 */
void printDebugInfo(void)
{
    if (CALIBRATION_MODE) return;

    printf("Vin:%.2f Vout:%.2f Ain:%.2f Aout:%.2f Win:%.2f Wout:%.2f Eff:%.1f D:%.1f M:%d\r\n",
        state.voltIn, state.voltOut, state.ampIn, state.ampOut, state.wattIn, state.wattOut,
        (state.wattIn > 0.1) ? (state.wattOut / state.wattIn * 100.0f) : 0.0f,
        dutyCycle, currentMode);
}

/**
  * @brief  This function is executed in case of error occurrence.
  */
void Error_Handler(void)
{
  __disable_irq();
  while (1) {}
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line){}
#endif /* USE_FULL_ASSERT */

/* USER CODE END 4 */