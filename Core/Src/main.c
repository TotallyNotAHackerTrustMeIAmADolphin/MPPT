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
#define TIMER_PERIOD ((uint16_t)480) // 100 kHz PWM frequency
#define DITHER_TABLE_SIZE 8          // 3 bits of dithering, has to be a power of 2

// ADC Stuff
#define ADC_CHANNEL_COUNT 6
#define ADC_CHANNEL_AVG_BITS 7
#define ADC_SAMPLE_COUNT (1 << ADC_CHANNEL_AVG_BITS)
#define ADC_BUF_LEN (ADC_CHANNEL_COUNT * ADC_SAMPLE_COUNT)

// conversion factor for converting ADC values to voltage with 200k and 5.1k resistors at 12 bit resolution of 3.3V
#define VOLT_IN_RAW_LOW 248
#define VOLT_IN_RAW_HIGH 876
#define VOLT_OUT_RAW_LOW 251
#define VOLT_OUT_RAW_HIGH 878
#define VOLT_REFERENCE_LOW 10.0f
#define VOLT_REFERENCE_HIGH 30.0f

// conversion factor from ADC values to current in mA with 0.333333 milli-ohm resistor at 12 bit resolution of 3.3V where 1.65V is the zero current point at 200 V/V gain
#define AMP_IN_RAW_LOW 1974
#define AMP_IN_RAW_HIGH 500
#define AMP_OUT_RAW_LOW 1974
#define AMP_OUT_RAW_HIGH 524
#define AMP_REFERENCE_LOW 0
#define AMP_REFERENCE_HIGH 4.1f
// #define AMP_IN_RAW_LOW 0
// #define AMP_IN_RAW_HIGH 1
// #define AMP_OUT_RAW_LOW 0
// #define AMP_OUT_RAW_HIGH 1
// #define AMP_REFERENCE_LOW 0
// #define AMP_REFERENCE_HIGH 1

// internal Temp sensor
#define V_REF_INT 1.2f
#define AVG_SLOPE (4.3f * 1000 * (4096 / V_REF_INT))
#define V30 (1.43f * (4096 / V_REF_INT))

// some max values
#define MAX_VOLTAGE_OUT 20
#define MAX_CURRENT_OUT 3
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
float dutyCycleConstantVoltage = 0;
float dutyCycleConstantCurrent = 0;
float dutyCycleMPPT = 0;
float averageWattChange = 0;
PID constantVoltage = {2.645532, 0.057958, 0.264580, 0, 0, 0, &voltOut, &dutyCycleConstantVoltage, 100, 0, 200};
PID constantCurrent = {23.241537, 14.503763, -0.626228, 0, 0, 0, &AmpOut, &dutyCycleConstantCurrent, 100, 0, 200};

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
void setPWM(float dutyCyclePct);
void updateDitherTable(uint16_t *pDitherTable, uint16_t desiredDutyCycle);

void readSensors();

// void constantVoltage(float voltage);
// void constantCurrent(float current);
void computePID(PID *pid);
void computeMPPT();
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

  bool autoTune = false;
  uint16_t sampleTime = 1280;

  PIDAutotuner constantVoltageTuner;
  PIDAutotuner_init(&constantVoltageTuner);
  PIDAutotuner_setOutputRange(&constantVoltageTuner, 40, 60);
  PIDAutotuner_setTargetInputValue(&constantVoltageTuner, 15);
  PIDAutotuner_setLoopInterval(&constantVoltageTuner, sampleTime);
  PIDAutotuner_setTuningCycles(&constantVoltageTuner, 100);
  PIDAutotuner_setZNMode(&constantVoltageTuner, ZNModeLessOvershoot);
  PIDAutotuner_startTuningLoop(&constantVoltageTuner, __HAL_TIM_GET_COUNTER(&htim6));

  PIDAutotuner constantCurrentTuner;
  PIDAutotuner_init(&constantCurrentTuner);
  PIDAutotuner_setOutputRange(&constantCurrentTuner, 20, 80);
  PIDAutotuner_setTargetInputValue(&constantCurrentTuner, 1.5);
  PIDAutotuner_setLoopInterval(&constantCurrentTuner, sampleTime);
  PIDAutotuner_setTuningCycles(&constantCurrentTuner, 100);
  PIDAutotuner_setZNMode(&constantCurrentTuner, ZNModeNoOvershoot);
  // PIDAutotuner_startTuningLoop(&constantCurrentTuner, __HAL_TIM_GET_COUNTER(&htim6));

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {

    if (bufferFull)
    {
      static uint8_t counter = 0;
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
      readSensors();

      if (autoTune)
      {
        if (!PIDAutotuner_isFinished(&constantVoltageTuner))
        {
          dutyCycle = PIDAutotuner_tunePID(&constantVoltageTuner, voltOut, time);
          // printf("In:\t%f V\t%f A\t%f W\tOUT:\t%f V\t%f A\t%f W\ttemp:\t%f\ttime:\t%u ms\n", voltIn, AmpIn, wattIn, voltOut, AmpOut, wattOut, tempMCU, timeDiff);
          // printf("Vin:%f\tVout:%f\tAin:%f\tAout:%f\tdutyCycle:%f\n", voltIn, voltOut, AmpIn * 10, AmpOut * 10, dutyCycle / 10);
          if (PIDAutotuner_isFinished(&constantVoltageTuner))
          {
            printf("voltage Kp: %f\tKi: %f\tKd: %f\ttimeDiff: %u\n", PIDAutotuner_getKp(&constantVoltageTuner), PIDAutotuner_getKi(&constantVoltageTuner), PIDAutotuner_getKd(&constantVoltageTuner), timeDiff);
            constantVoltage.Kp = PIDAutotuner_getKp(&constantVoltageTuner);
            constantVoltage.Ki = PIDAutotuner_getKi(&constantVoltageTuner);
            constantVoltage.Kd = PIDAutotuner_getKd(&constantVoltageTuner);
            PIDAutotuner_startTuningLoop(&constantCurrentTuner, __HAL_TIM_GET_COUNTER(&htim6));
            dutyCycle = 0;
          }
        }
        else if (!PIDAutotuner_isFinished(&constantCurrentTuner))
        {
          dutyCycle = PIDAutotuner_tunePID(&constantCurrentTuner, AmpOut, time);
          // printf("Vin:%f\tVout:%f\tAin:%f\tAout:%f\tdutyCycle:%f\n", voltIn, voltOut, AmpIn * 10, AmpOut * 10, dutyCycle / 10);
          if (PIDAutotuner_isFinished(&constantCurrentTuner))
          {
            printf("current Kp: %f\tKi: %f\tKd: %f\ttimeDiff: %u\n", PIDAutotuner_getKp(&constantCurrentTuner), PIDAutotuner_getKi(&constantCurrentTuner), PIDAutotuner_getKd(&constantCurrentTuner), timeDiff);
            constantCurrent.Kp = PIDAutotuner_getKp(&constantCurrentTuner);
            constantCurrent.Ki = PIDAutotuner_getKi(&constantCurrentTuner);
            constantCurrent.Kd = PIDAutotuner_getKd(&constantCurrentTuner);
          }
        }
        else
        {
          autoTune = false;
        }
      }

      else
      {
        constantVoltage.setPoint = 24;
        computePID(&constantVoltage);

        constantCurrent.setPoint = 3;
        computePID(&constantCurrent);

        if (counter % 32 == 0)
          computeMPPT();

        if (dutyCycleMPPT < dutyCycleConstantCurrent && dutyCycleMPPT < dutyCycleConstantVoltage)
        {
          dutyCycle = dutyCycleMPPT;
        }
        else if (dutyCycleConstantCurrent < dutyCycleConstantVoltage)
        {
          dutyCycle = dutyCycleConstantCurrent;
        }
        else
        {
          dutyCycle = dutyCycleConstantVoltage;
        }
      }

      if (voltIn < MIN_VOLTAGE_IN)
      {
        dutyCycle = 0;
        dutyCycleConstantVoltage = 0;
        dutyCycleConstantCurrent = 0;
        dutyCycleMPPT = 0;
      }
      setPWM(dutyCycle);

      if (counter % 32 == 0)
      {
        // computeMPPT();
        // printf("In:\t%f V\t%f A\t%f W\tOUT:\t%f V\t%f A\t%f W\ttemp:\t%f\ttime:\t%u ms\n", voltIn, AmpIn, wattIn, voltOut, AmpOut, wattOut, tempMCU, time - previousTime);
        printf("Vin:%f\tVout:%f\tAin:%f\tAout:%f\tWin:%f\tWout:%f\tdA:%f\tdV:%f\tdP:%f\n", voltIn, voltOut, AmpIn * 10, AmpOut * 10, wattIn, wattOut, dutyCycleConstantCurrent / 10, dutyCycleConstantVoltage / 10, dutyCycleMPPT / 10);
      }
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

  if (dutyCyclePct < 0)
  {
    dutyCyclePct = 0;
  }
  else if (dutyCyclePct > 160)
  {
    dutyCyclePct = 160;
  }

  if (dutyCyclePct < PWM_DEAD_BAND)
  { // Turn Off
    CH1Value = 0;
    CH2Value = 0;
  }
  else if (dutyCyclePct <= 100 + PWM_DEAD_BAND && dutyCyclePct >= 100 - PWM_DEAD_BAND)
  {                         // Just Passthrough
    CH1Value = timerPeriod; // 100% duty cycle for the high side Mosfet of the BOOST side
    CH2Value = timerPeriod; // 100% duty cycle for the BUCK side
  }
  else if (dutyCyclePct < 100 - PWM_DEAD_BAND)
  {                                              // Buck mode
    CH1Value = timerPeriod;                      // 100% duty cycle for the high side Mosfet of the BOOST side
    CH2Value = dutyCyclePct / 100 * timerPeriod; // duty cycle for the BUCK side
    // update the dithering tables in reverse order to avoid problems when switching from boost to buck mode
    updateDitherTable(ditherTableCH2, (uint16_t)roundf(CH2Value));
    updateDitherTable(ditherTableCH1, (uint16_t)roundf(CH1Value));
    return;
  }
  else if (dutyCyclePct > 100 + PWM_DEAD_BAND)
  {                                                        // Boost mode
    CH1Value = (200 - dutyCyclePct) / 100 * (timerPeriod); // duty cycle for the high side Mosfet of the BOOST side, but inverted logic (lower duty cycle = higher voltage)
    CH2Value = timerPeriod;                                // 100% duty cycle for the BUCK side
  }

  // __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, (uint16_t)roundf(CH1Value));
  // __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, (uint16_t)roundf(CH2Value));
  updateDitherTable(ditherTableCH1, (uint16_t)roundf(CH1Value));
  updateDitherTable(ditherTableCH2, (uint16_t)roundf(CH2Value));
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
  float AmpInTemp = (((((float)AmpIn_temp / ADC_SAMPLE_COUNT) - AMP_IN_RAW_LOW) * (AMP_REFERENCE_HIGH - AMP_REFERENCE_LOW)) / (AMP_IN_RAW_HIGH - AMP_IN_RAW_LOW)) + AMP_REFERENCE_LOW;
  // AmpIn = AmpIn * 0.95 + AmpInTemp * 0.05;
  AmpIn = AmpInTemp;
  float AmpOutTemp = (((((float)AmpOut_temp / ADC_SAMPLE_COUNT) - AMP_OUT_RAW_LOW) * (AMP_REFERENCE_HIGH - AMP_REFERENCE_LOW)) / (AMP_OUT_RAW_HIGH - AMP_OUT_RAW_LOW)) + AMP_REFERENCE_LOW;
  // AmpOut = AmpOut * 0.95 + AmpOutTemp * 0.05;
  AmpOut = AmpOutTemp;
  tempMofets = (float)(tempMofets_temp >> ADC_CHANNEL_AVG_BITS);
  tempMCU = (V30 - (float)(tempMCU_temp >> ADC_CHANNEL_AVG_BITS)) / AVG_SLOPE + 30;
  wattIn = voltIn * AmpIn;
  wattOut = voltOut * AmpOut;
}

void computePID(PID *pid)
{
  float error = pid->setPoint - *pid->input;
  float integral = pid->integral + error;
  float derivative = error - pid->previousError;

  integral = constrain(integral, -pid->maxIntegral, pid->maxIntegral); // limit the integral term to prevent windup

  *pid->output += pid->Kp * error + pid->Ki * integral + pid->Kd * derivative;
  *pid->output = constrain(*pid->output, pid->minOutput, pid->maxOutput); // limit the output to the PWM range

  pid->previousError = error;
  pid->integral = integral;
}

void computeMPPT()
{
  static float previousWattOut = 0;
  static bool direction = true;
  const float alpha = 0.1;
  const float deltaDutyCycle = 0.1;
  static float dutyCyclePlus = 0;

  float relativeChangeWattOut = ((wattOut - previousWattOut) / wattOut) * 100; // relative change in percent
  previousWattOut = wattOut;
  averageWattChange = averageWattChange * alpha + relativeChangeWattOut * (1 - alpha);
  relativeChangeWattOut = averageWattChange;

  if (relativeChangeWattOut < 0)
  {
    direction = !direction;
    averageWattChange = 0;
    dutyCyclePlus = deltaDutyCycle;
  }
  else{
    dutyCyclePlus += deltaDutyCycle;
  }

  if (direction)
  {
    dutyCycleMPPT += dutyCyclePlus;
  }
  else
  {
    dutyCycleMPPT -= dutyCyclePlus;
  }
  dutyCycleMPPT = constrain(dutyCycleMPPT, 10, 200);
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
