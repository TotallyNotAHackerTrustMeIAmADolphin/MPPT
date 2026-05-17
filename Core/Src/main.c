/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
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
#include <stdlib.h>

#include "pidautotuner.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
// PWM Stuff
#define TIMER_PERIOD ((uint16_t)240) // 100 kHz PWM frequency
#define DITHER_TABLE_SIZE 8          // 3 bits of dithering, has to be a power of 2

// ADC Stuff
#define ADC_CHANNEL_COUNT 6
#define ADC_CHANNEL_AVG_BITS 7
#define ADC_SAMPLE_COUNT (1 << ADC_CHANNEL_AVG_BITS)
#define ADC_BUF_LEN (ADC_CHANNEL_COUNT * ADC_SAMPLE_COUNT)

// turn on calibration mode
#define CALIBRATION_MODE false

// conversion factor for converting ADC values to voltage with 200k and 5.1k resistors at 12 bit resolution of 3.3V
#define VOLT_IN_RAW_LOW 950.603760
#define VOLT_IN_RAW_HIGH 2684.238525
#define VOLT_OUT_RAW_LOW 891.904358
#define VOLT_OUT_RAW_HIGH 2729.892090
#define VOLT_REFERENCE_LOW_MV 10000
#define VOLT_REFERENCE_HIGH_MV 30000

// conversion factor from ADC values to current in mA with 0.333333 milli-ohm resistor at 12 bit resolution of 3.3V where 1.65V is the zero current point at 200 V/V gain
#define AMP_IN_RAW_LOW 1987.640991
#define AMP_IN_RAW_HIGH 1674.981445
#define AMP_OUT_RAW_LOW 1986.062378
#define AMP_OUT_RAW_HIGH 1660.860840
#define AMP_IN_REFERENCE_LOW_MA 23
#define AMP_OUT_REFERENCE_LOW_MA 0
#define AMP_REFERENCE_HIGH_MA 3900

// internal Temp sensor
#define V_REF_INT_X1000 1200 // 1.2V * 1000
#define AVG_SLOPE_X1000 4300 // 4.3mV/C * 1000
#define V30_MV 1430           // 1.43V * 1000

// some max values
#define MAX_VOLTAGE_MV 80000
#define MAX_CURRENT_MA 20000
#define PWM_DEAD_BAND_TICKS 38 // ~2% of 1920
#define MIN_VOLTAGE_IN_MV 12000

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
#define constrain(amt, low, high) ((amt) < (low) ? (low) : ((amt) > (high) ? (high) : (amt)))
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
// data for the dithering table
uint16_t ditherTableCH1[DITHER_TABLE_SIZE];
uint16_t ditherTableCH2[DITHER_TABLE_SIZE];
uint16_t maxDutyCycle = (TIMER_PERIOD * DITHER_TABLE_SIZE);
uint8_t ditherBits = 3; // log2(DITHER_TABLE_SIZE)
int32_t dutyCycle_ticks = 0;

// data for the ADC
volatile uint16_t adc_buf[ADC_BUF_LEN];

// data read from Sensors
int32_t voltageIn_mV = 100000;
int32_t voltageOut_mV = 100000;
int32_t currentIn_mA = 100000;
int32_t currentOut_mA = 100000;
int32_t tempMofets_C_x100 = 0;
int32_t tempMCU_C_x100 = 0;
int64_t powerIn_uW = 0;
int64_t powerOut_uW = 0;
volatile uint8_t bufferFull = 0; // 0: idle, 1: first half, 2: second half

uint16_t REC = 2;
const int32_t currentCharging_mA = 2000;
const int32_t voltageBatteryMax_mV = 25200;
const int32_t voltageBatteryMin_mV = 3000 * 6;
const int32_t maxInputVoltage_mV = 80000;
const int32_t maxInputCurrent_mA = 20000;

// safety check Variables
bool overCurrentIn = false;
bool overCurrentOut = false;
bool overVoltageIn = false;
bool overVoltageOut = false;
bool underVoltageIn = false;
bool underVoltageOut = false;
uint8_t ERR = 0;

typedef struct
{
  int32_t Kp;
  int32_t Ki;
  int32_t Kd;
  int32_t previousError;
  int64_t integral;
  int32_t setPoint;
  int32_t *input;
  int32_t *output;
  int64_t maxIntegral;
  int32_t minOutput;
  int32_t maxOutput;
} PID;

int32_t dutyCycleConstantVoltage = 3840;
int32_t dutyCycleConstantCurrent = 3840;
int32_t dutyCycleMPPT = 0;
int32_t minDutyCycle_ticks = 0;
const int32_t maxDutyCycle_ticks = 3264; // 170% of 1920

PID constantVoltage = {100, 0, 10, 0, 0, voltageBatteryMax_mV, &voltageOut_mV, &dutyCycle_ticks, 1000000, 0, maxDutyCycle_ticks};
PID constantCurrent = {100, 0, 10, 0, 0, currentCharging_mA, &currentOut_mA, &dutyCycle_ticks, 1000000, 0, maxDutyCycle_ticks};

// --- State Machine for Global MPPT Sweep ---
typedef enum
{
  MPPT_TRACKING,
  MPPT_SWEEPING
} MPPT_State_t;

volatile MPPT_State_t mpptState = MPPT_TRACKING; // Start in normal tracking mode

// --- Sweep Configuration ---
const uint32_t SWEEP_INTERVAL_SECONDS = 300; // Sweep every 5 minutes
const int32_t SWEEP_STEP_SIZE_TICKS = 19;     // ~1.0% step
uint32_t sweepTriggerCounter = 0;

// --- Variables to store sweep results ---
int32_t sweepDutyCycle = 0;
int64_t sweepMaxPower_uW = 0;
int32_t sweepBestDutyCycle = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
void setPWM(int32_t dutyCycleTicks);
void updateDitherTable(uint16_t *pDitherTable, uint16_t desiredDutyCycle);

void readSensors(uint16_t offset);
void calibrateSensors(uint16_t offset);

void computePID(PID *pid);

void runMPPTAlgorithm();
void mpptPerturbAndObserve();

void Charging_Algorithm();
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

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
  HAL_TIM_PWM_Start_DMA(&htim1, TIM_CHANNEL_1, (uint32_t *)ditherTableCH1, DITHER_TABLE_SIZE);
  HAL_TIM_PWM_Start_DMA(&htim1, TIM_CHANNEL_2, (uint32_t *)ditherTableCH2, DITHER_TABLE_SIZE);

  HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_1);
  HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_2);

  HAL_ADC_Start_DMA(&hadc, (uint32_t *)adc_buf, ADC_BUF_LEN);

  HAL_TIM_Base_Start(&htim6);
  __HAL_TIM_GET_COUNTER(&htim6);

  dutyCycle_ticks = 0;
  setPWM(dutyCycle_ticks);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {

    if (bufferFull != 0) // if a half-buffer is ready
    {
      uint16_t offset = (bufferFull == 2) ? (ADC_BUF_LEN / 2) : 0;
      bufferFull = 0;

      static uint16_t counter = 0;
      static uint16_t previousTime = 0;
      uint16_t time = __HAL_TIM_GET_COUNTER(&htim6);
      uint16_t timeDiff = 0;
      if (time < previousTime)
      {
        timeDiff = (0xFFFF - previousTime) + time + 1;
      }
      else
      {
        timeDiff = time - previousTime;
      }
      previousTime = time;

      // read the sensor values
      readSensors(offset);

      // read the sensor RAW values for calibration
      if (CALIBRATION_MODE)
        calibrateSensors(offset);

      if (counter % 32 == 0)
      {
        if (REC > 0)
        {
          REC--;
        }
        if (!CALIBRATION_MODE)
          printf("Vin_mV:%ld\tVout_mV:%ld\tAin_mA:%ld\tAout_mA:%ld\tWin_uW:%lld\tWout_uW:%lld\tduty:%ld\tmin_duty:%ld\tEff_pct:%ld\tTemp:%ld\n", 
                 voltageIn_mV, voltageOut_mV, currentIn_mA, currentOut_mA, powerIn_uW, powerOut_uW, dutyCycle_ticks, minDutyCycle_ticks, 
                 (powerIn_uW > 10000) ? (int32_t)(powerOut_uW * 100 / powerIn_uW) : 0, tempMCU_C_x100 / 100);
      }

      // calculate the minimum duty cycle to avoid reverse current
      if (voltageOut_mV < voltageIn_mV && voltageIn_mV > 100) // Buck mode
      {
        minDutyCycle_ticks = (int32_t)((int64_t)voltageOut_mV * maxDutyCycle / voltageIn_mV);
        minDutyCycle_ticks = minDutyCycle_ticks - (maxDutyCycle / 100) + 19; 
      }
      else if (voltageOut_mV > 100) // boost mode
      {
        minDutyCycle_ticks = (int32_t)(2 * maxDutyCycle - ((int64_t)voltageIn_mV * maxDutyCycle / voltageOut_mV));
      }
      else 
      {
        minDutyCycle_ticks = 0;
      }
      
      minDutyCycle_ticks = constrain(minDutyCycle_ticks, 0, maxDutyCycle_ticks);

      // safety checks
      underVoltageIn = voltageIn_mV < MIN_VOLTAGE_IN_MV;
      underVoltageOut = voltageOut_mV < voltageBatteryMin_mV;
      overVoltageIn = voltageIn_mV > MAX_VOLTAGE_MV;
      overVoltageOut = voltageOut_mV > (voltageBatteryMax_mV - 50);
      overCurrentIn = currentIn_mA > maxInputCurrent_mA;
      overCurrentOut = currentOut_mA > (currentCharging_mA - 100);

      // safety check to see if Input Voltage is high enough
      if (overCurrentIn || overVoltageIn)
      {
        ERR = 1; // set Error counter to 1
      }
      else if (underVoltageIn)
      {
        REC = 3; // set Recovery counter to 3
      }
      if (overCurrentOut || overVoltageOut)
      {
        // If we are in CC or CV mode, reset the sweep timer and stay in tracking
        sweepTriggerCounter = 0;
        mpptState = MPPT_TRACKING; // Ensure we are in tracking mode

        if (overCurrentOut)
          computePID(&constantCurrent);
        if (overVoltageOut)
          computePID(&constantVoltage);
      }
      else if (counter % 8 == 0) // Run MPPT logic at a regular interval
      {
        // --- Sweep Trigger Logic ---
        if (mpptState == MPPT_TRACKING)
        {
          sweepTriggerCounter++;

          // Assuming the ADC callback happens roughly 200 times/sec,
          // and we run this block every 4 cycles (50 times/sec)
          const uint32_t calls_per_second = 50;
          if (sweepTriggerCounter > (SWEEP_INTERVAL_SECONDS * calls_per_second))
          {
            // Time to start a sweep!
            mpptState = MPPT_SWEEPING;
            sweepTriggerCounter = 0;

            // Initialize sweep variables
            sweepMaxPower_uW = 0;
            sweepBestDutyCycle = minDutyCycle_ticks;
            sweepDutyCycle = minDutyCycle_ticks; // Start sweep from the bottom
          }
        }

        // --- Run the main MPPT state machine ---
        runMPPTAlgorithm();
      }

      dutyCycle_ticks = constrain(dutyCycle_ticks, minDutyCycle_ticks, maxDutyCycle_ticks);

      if (CALIBRATION_MODE)
        dutyCycle_ticks = maxDutyCycle;
      if (REC > 0 || ERR > 0)
        dutyCycle_ticks = 0;

      setPWM(dutyCycle_ticks);
      counter++;
      timeDiff++; 
    }

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
   * in the RCC_OscInitTypeDef structure.
   */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI48;
  RCC_OscInitStruct.HSI48State = RCC_HSI48_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
   */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI48;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USB;
  PeriphClkInit.UsbClockSelection = RCC_USBCLKSOURCE_HSI48;

  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
// needed for printf
int _write(int file, char *ptr, int len)
{
  static uint8_t rc = USBD_OK;

  do
  {
    rc = CDC_Transmit_FS((uint8_t *)ptr, len);
  } while (USBD_BUSY == rc);

  if (USBD_FAIL == rc)
  {
    return 0;
  }
  return len;
}

void SerialPrint(char msg[])
{
  CDC_Transmit_FS((uint8_t *)msg, strlen(msg));
}

void setPWM(int32_t dutyCycleTicks)
{
  int32_t timerPeriod = maxDutyCycle;
  int32_t CH1Value = 0;
  int32_t CH2Value = 0;

  if (dutyCycleTicks < PWM_DEAD_BAND_TICKS)
  {
    dutyCycleTicks = 0;
  }
  else if (dutyCycleTicks > maxDutyCycle_ticks - PWM_DEAD_BAND_TICKS)
  {
    dutyCycleTicks = maxDutyCycle_ticks - PWM_DEAD_BAND_TICKS;
  }
  else if (dutyCycleTicks < timerPeriod && dutyCycleTicks > timerPeriod - PWM_DEAD_BAND_TICKS)
  {
    dutyCycleTicks = timerPeriod;
  }
  else if (dutyCycleTicks > timerPeriod && dutyCycleTicks < timerPeriod + PWM_DEAD_BAND_TICKS)
  {
    dutyCycleTicks = timerPeriod;
  }

  if (dutyCycleTicks == 0)
  { // Turn Off
    CH1Value = 0;
    CH2Value = 0;
    updateDitherTable(ditherTableCH2, (uint16_t)CH2Value);
    updateDitherTable(ditherTableCH1, (uint16_t)CH1Value);
  }
  else if (dutyCycleTicks == timerPeriod)
  {                         // Just Passthrough
    CH1Value = timerPeriod; 
    CH2Value = timerPeriod; 
    updateDitherTable(ditherTableCH2, (uint16_t)CH2Value);
    updateDitherTable(ditherTableCH1, (uint16_t)CH1Value);
  }
  else if (dutyCycleTicks < timerPeriod)
  {                         // Buck mode
    CH1Value = timerPeriod; 
    CH2Value = dutyCycleTicks;
    updateDitherTable(ditherTableCH2, (uint16_t)CH2Value);
    updateDitherTable(ditherTableCH1, (uint16_t)CH1Value);
    return;
  }
  else if (dutyCycleTicks > timerPeriod)
  {                                  // Boost mode
    CH1Value = 2 * timerPeriod - dutyCycleTicks; 
    CH2Value = timerPeriod;          
    updateDitherTable(ditherTableCH1, (uint16_t)CH1Value);
    updateDitherTable(ditherTableCH2, (uint16_t)CH2Value);
  }
}

/**
 * @brief Updates the dithering table with the desired duty cycle in tics
 */
void updateDitherTable(uint16_t *pDitherTable, uint16_t desiredDutyCycle)
{
  if (desiredDutyCycle > maxDutyCycle)
  {
    desiredDutyCycle = maxDutyCycle;
  }

  uint16_t baseDutyCycle = desiredDutyCycle >> ditherBits;           
  uint16_t DitherIndex = desiredDutyCycle & ((1 << ditherBits) - 1); 

  for (uint16_t table_index = 0; table_index < DITHER_TABLE_SIZE; table_index++)
  {
    if (table_index < DitherIndex)
    {
      pDitherTable[table_index] = baseDutyCycle + 1;
    }
    else
    {
      pDitherTable[table_index] = baseDutyCycle;
    }
  }
}

// Called when first half of buffer is filled
void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc)
{
  bufferFull = 1;
}

// Called when buffer is completely filled
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
  bufferFull = 2;
}

void readSensors(uint16_t offset)
{
  // temporary variables
  uint32_t voltIn_temp = 0;
  uint32_t voltOut_temp = 0;
  int32_t AmpIn_temp = 0;
  int32_t AmpOut_temp = 0;
  uint32_t tempMofets_temp = 0;
  uint32_t tempMCU_temp = 0;

  uint16_t end = offset + (ADC_BUF_LEN / 2);
  // sum the values of the ADC buffer
  for (int i = offset; i < end; i += ADC_CHANNEL_COUNT)
  {
    voltIn_temp += adc_buf[i];
    voltOut_temp += adc_buf[i + 2];
    AmpIn_temp += adc_buf[i + 1];
    AmpOut_temp += adc_buf[i + 3];
    tempMofets_temp += adc_buf[i + 4];
    tempMCU_temp += adc_buf[i + 5];
  }

  // Integer scaling for Voltages (mV)
  int64_t v_in_avg_x1000 = ((int64_t)voltIn_temp * 1000) / (ADC_SAMPLE_COUNT / 2);
  voltageIn_mV = (int32_t)(((v_in_avg_x1000 - (int64_t)(VOLT_IN_RAW_LOW * 1000)) * (VOLT_REFERENCE_HIGH_MV - VOLT_REFERENCE_LOW_MV)) / (int32_t)((VOLT_IN_RAW_HIGH - VOLT_IN_RAW_LOW) * 1000)) + VOLT_REFERENCE_LOW_MV;

  int64_t v_out_avg_x1000 = ((int64_t)voltOut_temp * 1000) / (ADC_SAMPLE_COUNT / 2);
  voltageOut_mV = (int32_t)(((v_out_avg_x1000 - (int64_t)(VOLT_OUT_RAW_LOW * 1000)) * (VOLT_REFERENCE_HIGH_MV - VOLT_REFERENCE_LOW_MV)) / (int32_t)((VOLT_OUT_RAW_HIGH - VOLT_OUT_RAW_LOW) * 1000)) + VOLT_REFERENCE_LOW_MV;

  // Integer scaling for Currents (mA)
  int64_t a_in_avg_x1000 = ((int64_t)AmpIn_temp * 1000) / (ADC_SAMPLE_COUNT / 2);
  currentIn_mA = (int32_t)(((a_in_avg_x1000 - (int64_t)(AMP_IN_RAW_LOW * 1000)) * (AMP_REFERENCE_HIGH_MA - AMP_IN_REFERENCE_LOW_MA)) / (int32_t)((AMP_IN_RAW_HIGH - AMP_IN_RAW_LOW) * 1000)) + AMP_IN_REFERENCE_LOW_MA;

  int64_t a_out_avg_x1000 = ((int64_t)AmpOut_temp * 1000) / (ADC_SAMPLE_COUNT / 2);
  currentOut_mA = (int32_t)(((a_out_avg_x1000 - (int64_t)(AMP_OUT_RAW_LOW * 1000)) * (AMP_REFERENCE_HIGH_MA - AMP_OUT_REFERENCE_LOW_MA)) / (int32_t)((AMP_OUT_RAW_HIGH - AMP_OUT_RAW_LOW) * 1000)) + AMP_OUT_REFERENCE_LOW_MA;

  tempMofets_C_x100 = (int32_t)((int64_t)tempMofets_temp * 100 / (ADC_SAMPLE_COUNT / 2));
  
  int32_t mcu_adc_avg = tempMCU_temp / (ADC_SAMPLE_COUNT / 2);
  tempMCU_C_x100 = (int32_t)((((int64_t)V30_MV - (mcu_adc_avg * 3300 / 4096)) * 1000 / (int32_t)(AVG_SLOPE_X1000 / 100)) + 3000);

  // Power in uW (mV * mA = uW)
  powerIn_uW = (int64_t)voltageIn_mV * currentIn_mA;
  powerOut_uW = (int64_t)voltageOut_mV * currentOut_mA;
}

void calibrateSensors(uint16_t offset)
{
  static int32_t voltInRaw_x1000 = 0;
  static int32_t voltOutRaw_x1000 = 0;
  static int32_t AmpInRaw_x1000 = 0;
  static int32_t AmpOutRaw_x1000 = 0;
  static uint16_t counter = 0;

  const int32_t alpha = 999;

  uint32_t voltIn_temp = 0;
  uint32_t voltOut_temp = 0;
  int32_t AmpIn_temp = 0;
  int32_t AmpOut_temp = 0;

  uint16_t end = offset + (ADC_BUF_LEN / 2);
  for (int i = offset; i < end; i += ADC_CHANNEL_COUNT)
  {
    voltIn_temp += adc_buf[i];
    voltOut_temp += adc_buf[i + 2];
    AmpIn_temp += adc_buf[i + 1];
    AmpOut_temp += adc_buf[i + 3];
  }

  int32_t v_in_now_x1000 = ((int64_t)voltIn_temp * 1000) / (ADC_SAMPLE_COUNT / 2);
  int32_t v_out_now_x1000 = ((int64_t)voltOut_temp * 1000) / (ADC_SAMPLE_COUNT / 2);
  int32_t a_in_now_x1000 = ((int64_t)AmpIn_temp * 1000) / (ADC_SAMPLE_COUNT / 2);
  int32_t a_out_now_x1000 = ((int64_t)AmpOut_temp * 1000) / (ADC_SAMPLE_COUNT / 2);

  voltInRaw_x1000 = (voltInRaw_x1000 * alpha + v_in_now_x1000 * (1000 - alpha)) / 1000;
  voltOutRaw_x1000 = (voltOutRaw_x1000 * alpha + v_out_now_x1000 * (1000 - alpha)) / 1000;
  AmpInRaw_x1000 = (AmpInRaw_x1000 * alpha + a_in_now_x1000 * (1000 - alpha)) / 1000;
  AmpOutRaw_x1000 = (AmpOutRaw_x1000 * alpha + a_out_now_x1000 * (1000 - alpha)) / 1000;

  if (counter % 64 == 0)
  {
    printf("Vin_raw:%ld\tVout_raw:%ld\tAin_raw:%ld\tAout_raw:%ld\n", voltInRaw_x1000, voltOutRaw_x1000, AmpInRaw_x1000, AmpOutRaw_x1000);
  }
  counter++;
}

void computePID(PID *pid)
{
  int32_t error = pid->setPoint - *pid->input;
  int64_t integral = pid->integral + error;
  int32_t derivative = error - pid->previousError;

  integral = constrain(integral, -pid->maxIntegral, pid->maxIntegral); 

  int64_t output_change = ((int64_t)pid->Kp * error) + ((int64_t)pid->Ki * integral) + ((int64_t)pid->Kd * derivative);

  *pid->output += (int32_t)(output_change / 1000);
  *pid->output = constrain(*pid->output, pid->minOutput, pid->maxOutput); 

  pid->previousError = error;
  pid->integral = integral;
}

void mpptPerturbAndObserve(void)
{
  static int64_t previousPowerIn_uW = 0;
  static bool direction = true; 

  const int32_t MIN_INPUT_VOLTAGE_MPPT_MV = 15000;
  if (voltageIn_mV < MIN_INPUT_VOLTAGE_MPPT_MV)
  {
    dutyCycle_ticks -= 19; 
    dutyCycle_ticks = constrain(dutyCycle_ticks, 0, maxDutyCycle_ticks);
    previousPowerIn_uW = powerIn_uW;
    return;
  }

  int64_t powerChange_uW = powerIn_uW - previousPowerIn_uW;
  const int64_t POWER_THRESHOLD_UW = 50000; 

  if (abs((int32_t)(powerChange_uW / 1000)) > (POWER_THRESHOLD_UW / 1000))
  {
    if (powerChange_uW < 0)
    {
      direction = !direction;
    }
  }

  const int32_t STEP_SIZE_TICKS = 8; 

  if (direction)
  {
    dutyCycle_ticks += STEP_SIZE_TICKS;
  }
  else
  {
    dutyCycle_ticks -= STEP_SIZE_TICKS;
  }

  dutyCycle_ticks = constrain(dutyCycle_ticks, minDutyCycle_ticks, maxDutyCycle_ticks);
  previousPowerIn_uW = powerIn_uW;
}

void runMPPTAlgorithm(void)
{
  switch (mpptState)
  {
  case MPPT_TRACKING:
    mpptPerturbAndObserve();
    break;

  case MPPT_SWEEPING:
    dutyCycle_ticks = sweepDutyCycle;

    if (powerIn_uW > sweepMaxPower_uW)
    {
      sweepMaxPower_uW = powerIn_uW;
      sweepBestDutyCycle = sweepDutyCycle;
    }

    sweepDutyCycle += SWEEP_STEP_SIZE_TICKS;

    if (sweepDutyCycle > maxDutyCycle_ticks)
    {
      dutyCycle_ticks = sweepBestDutyCycle;
      mpptState = MPPT_TRACKING;
    }
    break;
  }
}

/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef USE_FULL_ASSERT
/**
 * @brief  Reports the name of the source file and the source line number
 * @headroom: assert_param error line source number
 * @retval None
 */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
