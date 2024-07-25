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
#define ADC_SAMPLE_COUNT 50
#define ADC_BUF_LEN (ADC_CHANNEL_COUNT * ADC_SAMPLE_COUNT)

// conversion factor for converting ADC values to voltage with 200k and 5.1k resistors at 12 bit resolution of 3.3V
#define RESISTOR1 200.0f
#define RESISTOR2 16.0f
#define VOLTAGE_CONVERSION_FACTOR (3.3f / 4095.0f) * ((RESISTOR1 + RESISTOR2) / RESISTOR2)

// conversion factor from ADC values to current in mA with 0.333333 milli-ohm resistor at 12 bit resolution of 3.3V where 1.65V is the zero current point at 200 V/V gain
#define shunt_resistor 0.000333333333333333f
#define CURRENT_CONVERSION_FACTOR (-(3.3f / 4095.0f) / 200 / shunt_resistor)
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
// data for the dithering table
uint16_t ditherTableCH1[DITHER_TABLE_SIZE];
uint16_t ditherTableCH2[DITHER_TABLE_SIZE];
uint16_t maxDutyCycle = (TIMER_PERIOD * DITHER_TABLE_SIZE);
uint8_t ditherBits = 3; // log2(DITHER_TABLE_SIZE)

// data for the ADC
uint16_t adc_buf[ADC_BUF_LEN];

// data read from Sensors
float voltIn = 0;
float voltOut = 0;
float AmpIn = 0;
float AmpOut = 0;
float wattIn = 0;
float wattOut = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
void SerialPrint(char msg[]);
void setPWM(float dutyCyclePct);
void updateDitherTable(uint16_t *pDitherTable, uint16_t desiredDutyCycle);

void readSensors();
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
  HAL_TIM_PWM_Start_DMA(&htim1, TIM_CHANNEL_2, (uint32_t *)ditherTableCH2, DITHER_TABLE_SIZE);
  HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_1);
  HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_2);

  HAL_ADC_Start_DMA(&hadc, (uint32_t *)adc_buf, ADC_BUF_LEN);

  // HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
  // HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
  // HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_1);
  // HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_2);

  setPWM(100);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    // send a serial message via USB
    // char msg[] = "Hello World!\r\n";
    // CDC_Transmit_FS((uint8_t *)msg, strlen(msg));
    printf("Characters: %c %c\n", 'a', 65);
    printf("Decimals: %d %ld\n", 1977, 650000L);
    printf("Preceding with blanks: %10d\n", 1977);
    printf("Preceding with zeros: %010d\n", 1977);
    printf("Some different radices: %d %x %o %#x %#o\n", 100, 100, 100, 100, 100);
    printf("floats: %4.2f %+.0e %E\n", 3.1416, 3.1416, 3.1416);
    printf("Width trick: %*d\n", 5, 10);
    printf("%s\n", "A string");
    // HAL_ADC_Start_DMA(&hadc, (uint32_t *)adc_buf, ADC_BUF_LEN);
    readSensors();
    // setPWM((20 / voltIn) * 100);
    HAL_Delay(1000);

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
// needeed for printf
int _write(int file, char *ptr, int len)
{
  CDC_Transmit_FS((uint8_t *)ptr, len);
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
  else if (dutyCyclePct > 190)
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
  // unused
}

// Called when buffer is completely filled
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
  // unused
}

void readSensors()
{
  // write Values to the global variables
  for (int i = 0; i < ADC_BUF_LEN; i += ADC_CHANNEL_COUNT)
  {
    voltIn += adc_buf[i] * VOLTAGE_CONVERSION_FACTOR;
    voltOut += adc_buf[i + 2] * VOLTAGE_CONVERSION_FACTOR;
    AmpIn += ((int16_t)adc_buf[i + 1] - 1996.0) * CURRENT_CONVERSION_FACTOR;
    AmpOut += ((int16_t)adc_buf[i + 3] - 1996.0) * CURRENT_CONVERSION_FACTOR;
  }
  // calculate average
  voltIn /= ADC_SAMPLE_COUNT + 1;
  voltOut /= ADC_SAMPLE_COUNT + 1;
  AmpIn /= ADC_SAMPLE_COUNT + 1;
  AmpOut /= ADC_SAMPLE_COUNT + 1;
  wattIn = voltIn * AmpIn;
  wattOut = voltOut * AmpOut;
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
