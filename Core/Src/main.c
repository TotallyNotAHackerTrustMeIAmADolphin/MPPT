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
uint16_t adc_buf[ADC_BUF_LEN];

// data read from Sensors
float voltIn = 0;
float voltOut = 0;
float AmpIn = 0;
float AmpOut = 0;
float tempMofets = 0;
float tempMCU = 0;
float wattIn = 0;
float wattOut = 0;
volatile bool bufferFull = false;

uint32_t previousTime = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
void SerialPrint(char msg[]);
void setPWM(float dutyCyclePct);
void updateDitherTable(uint16_t *pDitherTable, uint16_t desiredDutyCycle);

void readSensors();

void constantVoltage(float voltage);
void constantCurrent(float current);
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
  /* USER CODE BEGIN 2 */
  HAL_TIM_PWM_Start_DMA(&htim1, TIM_CHANNEL_1, (uint32_t *)ditherTableCH1, DITHER_TABLE_SIZE);
  // HAL_TIMEx_PWMN_Start_DMA(&htim1, TIM_CHANNEL_1, (uint32_t *)ditherTableCH1, DITHER_TABLE_SIZE);
  HAL_TIM_PWM_Start_DMA(&htim1, TIM_CHANNEL_2, (uint32_t *)ditherTableCH2, DITHER_TABLE_SIZE);
  // HAL_TIMEx_PWMN_Start_DMA(&htim1, TIM_CHANNEL_2, (uint32_t *)ditherTableCH2, DITHER_TABLE_SIZE);

  HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_1);
  HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_2);

  HAL_ADC_Start_DMA(&hadc, (uint32_t *)adc_buf, ADC_BUF_LEN);

  // HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
  // HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
  // HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_1);
  // HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_2);

  // __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0);
  // __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, 0);

  dutyCycle = 0;
  setPWM(dutyCycle);
  // updateDitherTable(ditherTableCH1, 960);
  // updateDitherTable(ditherTableCH2, 960);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    // send a serial message via USB
    // char msg[] = "Hello World!\r\n";
    // CDC_Transmit_FS((uint8_t *)msg, strlen(msg));
    // printf("Characters: %c %c\n", 'a', 65);
    // printf("Decimals: %d %ld\n", 1977, 650000L);
    // printf("Preceding with blanks: %10d\n", 1977);
    // printf("Preceding with zeros: %010d\n", 1977);
    // printf("Some different radices: %d %x %o %#x %#o\n", 100, 100, 100, 100, 100);
    // printf("floats: %4.2f %+.0e %E\n", 3.1416, 3.1416, 3.1416);
    // printf("Width trick: %*d\n", 5, 10);
    // printf("%s\n", "A string");
    // print all the values
    // printf("In:\t%f V\t%f A\t%f W\tOUT:\t%f V\t%f A\t%f W\n", voltIn, AmpIn, wattIn, voltOut, AmpOut, wattOut);
    // HAL_ADC_Start_DMA(&hadc, (uint32_t *)adc_buf, ADC_BUF_LEN);
    // readSensors();
    // setPWM((20 / voltIn) * 100);
    // HAL_Delay(100);

    if (bufferFull)
    {
      static uint8_t counter = 0;
      bufferFull = false;
      uint32_t time = HAL_GetTick();
      readSensors();
      // constantVoltage(20);
      float maxVoltOut = 20;
      if (voltOut < maxVoltOut - 0.3)
      {
        constantCurrent(3);
      }
      else
      {
        constantVoltage(maxVoltOut);
      }
      // constantCurrent(1.9);
      if (counter % 8 == 0)
      {
        printf("In:\t%f V\t%f A\t%f W\tOUT:\t%f V\t%f A\t%f W\ttemp:\t%f\ttime:\t%lu ms\n", voltIn, AmpIn, wattIn, voltOut, AmpOut, wattOut, tempMCU, time - previousTime);
      }
      previousTime = time;
      counter++;
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

  if (dutyCyclePct < 2)
  {
    dutyCyclePct = 0;
  }
  else if (dutyCyclePct > 170)
  {
    dutyCyclePct = 190;
  }

  if (dutyCyclePct == 0)
  { // Turn Off
    CH1Value = 0;
    CH2Value = 0;
  }
  else if (dutyCyclePct == 100)
  {                         // Just Passthrough
    CH1Value = timerPeriod; // 100% duty cycle for the high side Mosfet of the BOOST side
    CH2Value = timerPeriod; // 100% duty cycle for the BUCK side
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
  AmpIn = (((((float)AmpIn_temp / ADC_SAMPLE_COUNT) - AMP_IN_RAW_LOW) * (AMP_REFERENCE_HIGH - AMP_REFERENCE_LOW)) / (AMP_IN_RAW_HIGH - AMP_IN_RAW_LOW)) + AMP_REFERENCE_LOW;
  AmpOut = (((((float)AmpOut_temp / ADC_SAMPLE_COUNT) - AMP_OUT_RAW_LOW) * (AMP_REFERENCE_HIGH - AMP_REFERENCE_LOW)) / (AMP_OUT_RAW_HIGH - AMP_OUT_RAW_LOW)) + AMP_REFERENCE_LOW;
  tempMofets = (float)(tempMofets_temp >> ADC_CHANNEL_AVG_BITS);
  tempMCU = (V30 - (float)(tempMCU_temp >> ADC_CHANNEL_AVG_BITS)) / AVG_SLOPE + 30;
  wattIn = voltIn * AmpIn;
  wattOut = voltOut * AmpOut;
}

void constantVoltage(float voltage)
{
  float error = voltage - voltOut;
  dutyCycle += error * 1;
  dutyCycle = constrain(dutyCycle, 0, 200);
  setPWM(dutyCycle);
}
void constantCurrent(float current)
{
  float error = current - AmpOut;
  dutyCycle += error * 50;
  dutyCycle = constrain(dutyCycle, 0, 200);
  setPWM(dutyCycle);
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
