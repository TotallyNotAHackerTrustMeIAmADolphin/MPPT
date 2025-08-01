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
#define VOLT_REFERENCE_LOW 10.0f
#define VOLT_REFERENCE_HIGH 30.0f

// conversion factor from ADC values to current in mA with 0.333333 milli-ohm resistor at 12 bit resolution of 3.3V where 1.65V is the zero current point at 200 V/V gain
#define AMP_IN_RAW_LOW 1987.640991
#define AMP_IN_RAW_HIGH 1674.981445
#define AMP_OUT_RAW_LOW 1986.062378
#define AMP_OUT_RAW_HIGH 1660.860840
#define AMP_IN_REFERENCE_LOW 0.023f
#define AMP_OUT_REFERENCE_LOW 0.0
#define AMP_REFERENCE_HIGH 3.9f

// internal Temp sensor
#define V_REF_INT 1.2f
#define AVG_SLOPE (4.3f * 1000 * (4096 / V_REF_INT))
#define V30 (1.43f * (4096 / V_REF_INT))

// some max values
#define MAX_VOLTAGE 80
#define MAX_CURRENT 20
#define PWM_DEAD_BAND 2 // percent
#define MIN_VOLTAGE_IN 12

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
float dutyCycle = 0;

// data for the ADC
volatile uint16_t adc_buf[ADC_BUF_LEN];

// data read from Sensors
float voltIn = 100;
float voltOut = 100;
float AmpIn = 100;
float AmpOut = 100;
float tempMofets = 0;
float tempMCU = 0;
float wattIn = 0;
float wattOut = 0;
volatile bool bufferFull = false;

uint16_t REC = 2;
const float currentCharging = 2;
const float voltageBatteryMax = 25.2;
const float voltageBatteryMin = 3 * 6;
const float maxInputVoltage = 80;
const float maxInputCurrent = 20;

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
  float Kp;
  float Ki;
  float Kd;
  float previousError;
  float integral;
  float setPoint;
  float *input;
  float *output;
  float maxIntegral;
  float minOutput;
  float maxOutput;
} PID;

float dutyCycleConstantVoltage = 200;
float dutyCycleConstantCurrent = 200;
float dutyCycleMPPT = 0;
float minDutyCycle = 0;
const float maxDutyCyclePercent = 170;

PID constantVoltage = {0.1, 0, 0.0001, 0, 0, voltageBatteryMax, &voltOut, &dutyCycle, 1, 0, maxDutyCyclePercent};
PID constantCurrent = {0.1, 0, 0.0001, 0, 0, currentCharging, &AmpOut, &dutyCycle, 1, 0, maxDutyCyclePercent};
// PID constantVoltage = {0.1, 0.0001, 0.0001, 0, 0, 0, &voltOut, &dutyCycleConstantVoltage, 100, 0, 200};
// PID constantCurrent = {2.0, 0.0001, 0.00001, 0, 0, 0, &AmpOut, &dutyCycleConstantCurrent, 1, 0, 200};

// --- State Machine for Global MPPT Sweep ---
typedef enum
{
  MPPT_TRACKING,
  MPPT_SWEEPING
} MPPT_State_t;

volatile MPPT_State_t mpptState = MPPT_TRACKING; // Start in normal tracking mode

// --- Sweep Configuration ---
const uint32_t SWEEP_INTERVAL_SECONDS = 300; // Sweep every 5 minutes
const float SWEEP_STEP_SIZE = 1.0;           // How big are the duty cycle steps during a sweep
uint32_t sweepTriggerCounter = 0;

// --- Variables to store sweep results ---
float sweepDutyCycle = 0;
float sweepMaxPower = 0;
float sweepBestDutyCycle = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
void setPWM(float dutyCyclePct);
void updateDitherTable(uint16_t *pDitherTable, uint16_t desiredDutyCycle);

void readSensors();
void calibrateSensors();

// void constantVoltage(float voltage);
// void constantCurrent(float current);
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
  // HAL_TIMEx_PWMN_Start_DMA(&htim1, TIM_CHANNEL_1, (uint32_t *)ditherTableCH1, DITHER_TABLE_SIZE);
  HAL_TIM_PWM_Start_DMA(&htim1, TIM_CHANNEL_2, (uint32_t *)ditherTableCH2, DITHER_TABLE_SIZE);
  // HAL_TIMEx_PWMN_Start_DMA(&htim1, TIM_CHANNEL_2, (uint32_t *)ditherTableCH2, DITHER_TABLE_SIZE);

  HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_1);
  HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_2);

  HAL_ADC_Start_DMA(&hadc, (uint32_t *)adc_buf, ADC_BUF_LEN);

  HAL_TIM_Base_Start(&htim6);
  __HAL_TIM_GET_COUNTER(&htim6);

  // HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
  // HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
  // HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_1);
  // HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_2);

  // __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0);
  // __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, 0);

  dutyCycle = 0;
  setPWM(dutyCycle);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {

    if (bufferFull) // if the buffer is full, process the data
    {
      static uint16_t counter = 0;
      bufferFull = false;
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
      readSensors();

      // read the sensor RAW values for calibration
      if (CALIBRATION_MODE)
        calibrateSensors();

      if (counter % 32 == 0)
      {
        if (REC > 0)
        {
          REC--;
        }
        if (!CALIBRATION_MODE)
          printf("Vin:%f\tVout:%f\tAin:%f\tAout:%f\tWin:%f\tWout:%f\tdA:%f\tdV:%f\tdP:%f\n", voltIn, voltOut, AmpIn, AmpOut, wattIn, wattOut, dutyCycle, minDutyCycle, wattOut / wattIn * 100);
      }

      // calculate the minimum duty cycle to avoid reverse current
      if (voltOut < voltIn) // Buck mode
      {
        minDutyCycle = voltOut / voltIn * 100;
        minDutyCycle = minDutyCycle - (100 / minDutyCycle) + 1;
      }
      else // boost mode
      {
        minDutyCycle = 198 - voltIn / voltOut * 100;
      }

      // safety checks
      underVoltageIn = voltIn < MIN_VOLTAGE_IN;
      underVoltageOut = voltOut < voltageBatteryMin;
      overVoltageIn = voltIn > maxInputVoltage;
      overVoltageOut = voltOut > (voltageBatteryMax - 0.05);
      overCurrentIn = AmpIn > maxInputCurrent;
      overCurrentOut = AmpOut > (currentCharging - 0.1);

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
            sweepMaxPower = 0;
            sweepBestDutyCycle = minDutyCycle;
            sweepDutyCycle = minDutyCycle; // Start sweep from the bottom
          }
        }

        // --- Run the main MPPT state machine ---
        runMPPTAlgorithm();
      }

      dutyCycle = constrain(dutyCycle, minDutyCycle, maxDutyCyclePercent);

      if (CALIBRATION_MODE)
        dutyCycle = 100;
      if (REC > 0 || ERR > 0)
        dutyCycle = 0;

      setPWM(dutyCycle);
      counter++;
      timeDiff++; // just here so the compiler doesn't complain about unused variable
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
    /// NOTE: Should never reach here.
    /// TODO: Handle this error.
    return 0;
  }
  return len;
}

void SerialPrint(char msg[])
{
  CDC_Transmit_FS((uint8_t *)msg, strlen(msg));
}

void setPWM(float dutyCyclePct)
{
  // float timerPeriod = TIMER_PERIOD;
  float timerPeriod = maxDutyCycle;
  float CH1Value = 0;
  float CH2Value = 0;

  if (dutyCyclePct < PWM_DEAD_BAND)
  {
    dutyCyclePct = 0;
  }
  else if (dutyCyclePct > maxDutyCyclePercent - PWM_DEAD_BAND)
  {
    dutyCyclePct = maxDutyCyclePercent - PWM_DEAD_BAND;
  }
  else if (dutyCyclePct < 100 && dutyCyclePct > 100 - PWM_DEAD_BAND)
  {
    // dutyCyclePct = 100 - PWM_DEAD_BAND;
    dutyCyclePct = 100;
  }
  else if (dutyCyclePct > 100 && dutyCyclePct < 100 + PWM_DEAD_BAND)
  {
    // dutyCyclePct = 100 + PWM_DEAD_BAND;
    dutyCyclePct = 100;
  }

  if (dutyCyclePct == 0)
  { // Turn Off
    CH1Value = 0;
    CH2Value = 0;
    updateDitherTable(ditherTableCH2, (uint16_t)roundf(CH2Value));
    updateDitherTable(ditherTableCH1, (uint16_t)roundf(CH1Value));
  }
  else if (dutyCyclePct == 100)
  {                         // Just Passthrough
    CH1Value = timerPeriod; // 100% duty cycle for the high side Mosfet of the BOOST side
    CH2Value = timerPeriod; // 100% duty cycle for the BUCK side
    updateDitherTable(ditherTableCH2, (uint16_t)roundf(CH2Value));
    updateDitherTable(ditherTableCH1, (uint16_t)roundf(CH1Value));
  }
  else if (dutyCyclePct < 100)
  {                                              // Buck mode
    CH1Value = timerPeriod;                      // 100% duty cycle for the high side Mosfet of the BOOST side
    CH2Value = dutyCyclePct / 100 * timerPeriod; // duty cycle for the BUCK side
    // update the dithering tables in reverse order to avoid problems when switching from boost to buck mode
    updateDitherTable(ditherTableCH2, (uint16_t)roundf(CH2Value));
    updateDitherTable(ditherTableCH1, (uint16_t)roundf(CH1Value));
    return;
  }
  else if (dutyCyclePct > 100)
  {                                                        // Boost mode
    CH1Value = (200 - dutyCyclePct) / 100 * (timerPeriod); // duty cycle for the high side Mosfet of the BOOST side, but inverted logic (lower duty cycle = higher voltage)
    CH2Value = timerPeriod;                                // 100% duty cycle for the BUCK side
    updateDitherTable(ditherTableCH1, (uint16_t)roundf(CH1Value));
    updateDitherTable(ditherTableCH2, (uint16_t)roundf(CH2Value));
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

  uint16_t baseDutyCycle = desiredDutyCycle >> ditherBits;           // Upper 7 bits for base duty cycle
  uint16_t DitherIndex = desiredDutyCycle & ((1 << ditherBits) - 1); // Lower 3 bits for dither index

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
  bufferFull = true;
}

// Called when buffer is completely filled
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
  // bufferFull = true;
}

void readSensors()
{
  // temporary variables
  uint32_t voltIn_temp = 0;
  uint32_t voltOut_temp = 0;
  int32_t AmpIn_temp = 0;
  int32_t AmpOut_temp = 0;
  uint32_t tempMofets_temp = 0;
  uint32_t tempMCU_temp = 0;
  // sum the values of the ADC buffer
  for (int i = 0; i < ADC_BUF_LEN; i += ADC_CHANNEL_COUNT)
  {
    voltIn_temp += adc_buf[i];
    voltOut_temp += adc_buf[i + 2];
    AmpIn_temp += adc_buf[i + 1];
    AmpOut_temp += adc_buf[i + 3];
    tempMofets_temp += adc_buf[i + 4];
    tempMCU_temp += adc_buf[i + 5];
  }
  // calculate average and convert to real values
  voltIn = (((((float)voltIn_temp / ADC_SAMPLE_COUNT) - VOLT_IN_RAW_LOW) * (VOLT_REFERENCE_HIGH - VOLT_REFERENCE_LOW)) / (VOLT_IN_RAW_HIGH - VOLT_IN_RAW_LOW)) + VOLT_REFERENCE_LOW;
  voltOut = (((((float)voltOut_temp / ADC_SAMPLE_COUNT) - VOLT_OUT_RAW_LOW) * (VOLT_REFERENCE_HIGH - VOLT_REFERENCE_LOW)) / (VOLT_OUT_RAW_HIGH - VOLT_OUT_RAW_LOW)) + VOLT_REFERENCE_LOW;
  AmpIn = (((((float)AmpIn_temp / ADC_SAMPLE_COUNT) - AMP_IN_RAW_LOW) * (AMP_REFERENCE_HIGH - AMP_IN_REFERENCE_LOW)) / (AMP_IN_RAW_HIGH - AMP_IN_RAW_LOW)) + AMP_IN_REFERENCE_LOW;
  AmpOut = (((((float)AmpOut_temp / ADC_SAMPLE_COUNT) - AMP_OUT_RAW_LOW) * (AMP_REFERENCE_HIGH - AMP_OUT_REFERENCE_LOW)) / (AMP_OUT_RAW_HIGH - AMP_OUT_RAW_LOW)) + AMP_OUT_REFERENCE_LOW;
  tempMofets = (float)(tempMofets_temp >> ADC_CHANNEL_AVG_BITS);
  tempMCU = (V30 - (float)(tempMCU_temp >> ADC_CHANNEL_AVG_BITS)) / AVG_SLOPE + 30;
  wattIn = voltIn * AmpIn;
  wattOut = voltOut * AmpOut;
}

void calibrateSensors()
{
  static float voltInRaw = 0;
  static float voltOutRaw = 0;
  static float AmpInRaw = 0;
  static float AmpOutRaw = 0;
  static uint16_t counter = 0;

  // constants
  const float alpha = 0.999;

  // temporary variables
  uint32_t voltIn_temp = 0;
  uint32_t voltOut_temp = 0;
  int32_t AmpIn_temp = 0;
  int32_t AmpOut_temp = 0;
  uint32_t tempMofets_temp = 0;
  uint32_t tempMCU_temp = 0;

  // sum the values of the ADC buffer
  for (int i = 0; i < ADC_BUF_LEN; i += ADC_CHANNEL_COUNT)
  {
    voltIn_temp += adc_buf[i];
    voltOut_temp += adc_buf[i + 2];
    AmpIn_temp += adc_buf[i + 1];
    AmpOut_temp += adc_buf[i + 3];
    tempMofets_temp += adc_buf[i + 4];
    tempMCU_temp += adc_buf[i + 5];
  }

  voltInRaw = voltInRaw * alpha + ((float)voltIn_temp / ADC_SAMPLE_COUNT) * (1 - alpha);
  voltOutRaw = voltOutRaw * alpha + ((float)voltOut_temp / ADC_SAMPLE_COUNT) * (1 - alpha);
  AmpInRaw = AmpInRaw * alpha + ((float)AmpIn_temp / ADC_SAMPLE_COUNT) * (1 - alpha);
  AmpOutRaw = AmpOutRaw * alpha + ((float)AmpOut_temp / ADC_SAMPLE_COUNT) * (1 - alpha);

  if (counter % 64 == 0)
  {
    printf("Vin:%f\tVout:%f\tAin:%f\tAout:%f\n", voltInRaw, voltOutRaw, AmpInRaw, AmpOutRaw);
  }
  counter++;
}

void computePID(PID *pid)
{
  float error = pid->setPoint - *pid->input;
  float integral = pid->integral + error;
  float derivative = error - pid->previousError;

  integral = constrain(integral, -pid->maxIntegral, pid->maxIntegral); // limit the integral term to prevent windup

  *pid->output += pid->Kp * error + pid->Ki * integral + pid->Kd * derivative;
  //*pid->output = constrain(*pid->output, pid->minOutput, pid->maxOutput); // limit the output to the PWM range

  pid->previousError = error;
  pid->integral = integral;
}

void mpptPerturbAndObserve(void)
{
  // The entire contents of your working computeMPPT function go here
  static float previousWattIn = 0;
  static bool direction = true; // true = increase duty cycle, false = decrease

  const float MIN_INPUT_VOLTAGE_MPPT = 15.0;
  if (voltIn < MIN_INPUT_VOLTAGE_MPPT)
  {
    dutyCycle -= 1.0;
    dutyCycle = constrain(dutyCycle, 0, maxDutyCyclePercent);
    previousWattIn = wattIn;
    return;
  }

  float powerChange = wattIn - previousWattIn;
  const float POWER_THRESHOLD = 0.05;

  if (fabs(powerChange) > POWER_THRESHOLD)
  {
    if (powerChange < 0)
    {
      direction = !direction;
    }
  }

  const float STEP_SIZE = 0.4;

  if (direction)
  {
    dutyCycle += STEP_SIZE;
  }
  else
  {
    dutyCycle -= STEP_SIZE;
  }

  dutyCycle = constrain(dutyCycle, minDutyCycle, maxDutyCyclePercent);
  previousWattIn = wattIn;
}

void runMPPTAlgorithm(void)
{
  switch (mpptState)
  {
  case MPPT_TRACKING:
    // Perform normal, fast P&O tracking
    mpptPerturbAndObserve();
    break;

  case MPPT_SWEEPING:
    // Set the duty cycle for the current step of the sweep
    dutyCycle = sweepDutyCycle;

    // Give the system a moment to stabilize before measuring power
    // (In this code, the next loop iteration provides this delay)

    // Check if the current power is the highest we've seen so far
    if (wattIn > sweepMaxPower)
    {
      sweepMaxPower = wattIn;
      sweepBestDutyCycle = sweepDutyCycle;
    }

    // Move to the next duty cycle step
    sweepDutyCycle += SWEEP_STEP_SIZE;

    // Check if the sweep is finished
    if (sweepDutyCycle > maxDutyCyclePercent)
    {
      // Sweep is done! Jump to the best duty cycle found.
      dutyCycle = sweepBestDutyCycle;

      // Return to normal tracking mode
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
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef USE_FULL_ASSERT
/**
 * @brief  Reports the name of the source file and the source line number
 *         where the assert_param error has occurred.
 * @param  file: pointer to the source file name
 * @param  line: assert_param error line source number
 * @retval None
 */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
