#include <Arduino.h>
#include <constants.h>

extern "C" void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
   * in the RCC_OscInitTypeDef structure.
   */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI14 | RCC_OSCILLATORTYPE_HSI48;
  RCC_OscInitStruct.HSI48State = RCC_HSI48_ON;
  RCC_OscInitStruct.HSI14State = RCC_HSI14_ON;
  RCC_OscInitStruct.HSI14CalibrationValue = 16;
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

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  if (htim->Instance == TIM1)
  {
    /* USER CODE BEGIN TIM1_MspPostInit 0 */

    /* USER CODE END TIM1_MspPostInit 0 */

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    /**TIM1 GPIO Configuration
    PA7     ------> TIM1_CH1N
    PB0     ------> TIM1_CH2N
    PA8     ------> TIM1_CH1
    PA9     ------> TIM1_CH2
    */
    GPIO_InitStruct.Pin = GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF2_TIM1;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_0;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF2_TIM1;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* USER CODE BEGIN TIM1_MspPostInit 1 */

    /* USER CODE END TIM1_MspPostInit 1 */
  }
}

TIM_HandleTypeDef htim1;
DMA_HandleTypeDef hdma_tim1_ch1;
DMA_HandleTypeDef hdma_tim1_ch2;

/**
 * @brief TIM1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM1_Init(void)
{

  /* USER CODE BEGIN TIM1_Init 0 */

  /* USER CODE END TIM1_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};
  TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};

  /* USER CODE BEGIN TIM1_Init 1 */

  /* USER CODE END TIM1_Init 1 */
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 0;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 479;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim1, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
  sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
  sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
  sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
  sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
  sBreakDeadTimeConfig.DeadTime = 2;
  sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
  sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
  sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
  if (HAL_TIMEx_ConfigBreakDeadTime(&htim1, &sBreakDeadTimeConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM1_Init 2 */

  /* USER CODE END TIM1_Init 2 */
  HAL_TIM_MspPostInit(&htim1);
}

/**
 * Enable DMA controller clock
 */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Channel2_3_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel2_3_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel2_3_IRQn);
}

/**
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init(void)
{
  /* USER CODE BEGIN MX_GPIO_Init_1 */
  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /* USER CODE BEGIN MX_GPIO_Init_2 */
  /* USER CODE END MX_GPIO_Init_2 */
}

#define TIMER_PERIOD ((uint16_t)479) // 100 kHz PWM frequency
#define DITHER_TABLE_SIZE 8          // 3 bits of dithering, has to be a power of 2
// data for the dithering table
uint16_t ditherTableCH1[2 * DITHER_TABLE_SIZE];
uint16_t ditherTableCH2[2 * DITHER_TABLE_SIZE];
uint16_t maxDutyCycle = ((TIMER_PERIOD + 1) * DITHER_TABLE_SIZE) - 1;
uint8_t ditherBits = 3; // log2(DITHER_TABLE_SIZE)

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
/*
 * @brief Sets the PWM as value from 0 to 100 %
 */
// void setPWM(uint32_t channel, float dutyCyclePct)
// {
//   if (dutyCyclePct < 0)
//   {
//     dutyCyclePct = 0;
//   }
//   else if (dutyCyclePct > 100)
//   {
//     dutyCyclePct = 100;
//   }

//   uint16_t dutyCycle = (uint16_t)roundf(dutyCyclePct / 100 * (float)maxDutyCycle);

//   if (channel == TIM_CHANNEL_1)
//   {
//     updateDitherTable(ditherTableCH1, dutyCycle);
//   }
//   else if (channel == TIM_CHANNEL_2)
//   {
//     updateDitherTable(ditherTableCH2, dutyCycle);
//   }
// }

void setPWM(uint8_t channel, float dutyCyclePct)
{
  if (dutyCyclePct < 0)
  {
    dutyCyclePct = 0;
  }
  else if (dutyCyclePct > 100)
  {
    dutyCyclePct = 100;
  }
  uint16_t value = (uint16_t)roundf(dutyCyclePct / 100 * (float)(TIMER_PERIOD + 1));
  // inverse the value for channel 1 because i did an oopsie in the design
  if (channel == TIM_CHANNEL_1)
  {
    value = map(value, 0, TIMER_PERIOD + 1, TIMER_PERIOD + 1, 0);
  }
  __HAL_TIM_SET_COMPARE(&htim1, channel, value);
}

void initPWM()
{
  // setPWM(TIM_CHANNEL_1, 0);
  // setPWM(TIM_CHANNEL_2, 0);

  MX_GPIO_Init();
  MX_DMA_Init();
  MX_TIM1_Init();

  // taken from stm32f0xx_hal_msp.c function HAL_TIM_Base_MspInit
  // __HAL_RCC_TIM1_CLK_ENABLE();

  // /* TIM1 DMA Init */
  // /* TIM1_CH1 Init */
  // hdma_tim1_ch1.Instance = DMA1_Channel2;
  // hdma_tim1_ch1.Init.Direction = DMA_MEMORY_TO_PERIPH;
  // hdma_tim1_ch1.Init.PeriphInc = DMA_PINC_DISABLE;
  // hdma_tim1_ch1.Init.MemInc = DMA_MINC_DISABLE;
  // hdma_tim1_ch1.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
  // hdma_tim1_ch1.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
  // hdma_tim1_ch1.Init.Mode = DMA_CIRCULAR;
  // hdma_tim1_ch1.Init.Priority = DMA_PRIORITY_LOW;
  // if (HAL_DMA_Init(&hdma_tim1_ch1) != HAL_OK)
  // {
  //   Error_Handler();
  // }

  // __HAL_LINKDMA(&htim1, hdma[TIM_DMA_ID_CC1], hdma_tim1_ch1);

  // /* TIM1_CH2 Init */
  // hdma_tim1_ch2.Instance = DMA1_Channel3;
  // hdma_tim1_ch2.Init.Direction = DMA_MEMORY_TO_PERIPH;
  // hdma_tim1_ch2.Init.PeriphInc = DMA_PINC_DISABLE;
  // hdma_tim1_ch2.Init.MemInc = DMA_MINC_DISABLE;
  // hdma_tim1_ch2.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
  // hdma_tim1_ch2.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
  // hdma_tim1_ch2.Init.Mode = DMA_CIRCULAR;
  // hdma_tim1_ch2.Init.Priority = DMA_PRIORITY_LOW;
  // if (HAL_DMA_Init(&hdma_tim1_ch2) != HAL_OK)
  // {
  //   Error_Handler();
  // }

  // __HAL_LINKDMA(&htim1, hdma[TIM_DMA_ID_CC2], hdma_tim1_ch2);
  // end of taken from stm32f0xx_hal_msp.c function HAL_TIM_Base_MspInit

  // HAL_TIM_PWM_Start_DMA(&htim1, TIM_CHANNEL_1, (uint32_t *)ditherTableCH1, 2 * DITHER_TABLE_SIZE);
  // HAL_TIMEx_PWMN_Start_DMA(&htim1, TIM_CHANNEL_1, (uint32_t *)ditherTableCH1, 2 * DITHER_TABLE_SIZE);
  // HAL_TIM_PWM_Start_DMA(&htim1, TIM_CHANNEL_2, (uint32_t *)ditherTableCH2, 2 * DITHER_TABLE_SIZE);
  // HAL_TIMEx_PWMN_Start_DMA(&htim1, TIM_CHANNEL_2, (uint32_t *)ditherTableCH2, 2 * DITHER_TABLE_SIZE);

  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
  HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_1);
  HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_2);

  setPWM(TIM_CHANNEL_1, 0);
  setPWM(TIM_CHANNEL_2, 0);
}

void setup()
{
  // SystemClock_Config();
  initPWM();
  setPWM(TIM_CHANNEL_2, 100);
  setPWM(TIM_CHANNEL_1, 20);

  SerialUSB.begin(115200);
  // setPWM(TIM_CHANNEL_1, 0);
  // setPWM(TIM_CHANNEL_2, 0);
  pinMode(PIN_LED, OUTPUT);
  // pinMode(PIN_Q5, OUTPUT);
  // pinMode(PIN_Q6, OUTPUT);
  // pinMode(PIN_Q7, OUTPUT);
  // pinMode(PIN_Q8, OUTPUT);
  // digitalWrite(PIN_Q5, LOW);
  // digitalWrite(PIN_Q6, LOW);
  // digitalWrite(PIN_Q7, LOW);
  // digitalWrite(PIN_Q8, LOW);
  analogReadResolution(12);
}

float dutyCycle = 0;
float targetVoltage = 24.0f;

void loop()
{
  // calculate current values
  float currentIn = analogRead(PIN_A2); // mA
  currentIn -= 2048;                    // 12 bit ADC
  currentIn *= CURRENT_CONVERSION_FACTOR;
  float currentOut = analogRead(PIN_A4); // mA
  currentOut -= 2048;
  currentOut *= CURRENT_CONVERSION_FACTOR;

  // calculate voltage values
  float voltageIn = analogRead(PIN_A1) * VOLTAGE_CONVERSION_FACTOR;
  float voltageOut = analogRead(PIN_A3) * VOLTAGE_CONVERSION_FACTOR * 1.01f;

  if (voltageOut > targetVoltage)
  {
    dutyCycle -= 1;
  }
  else if (voltageOut < targetVoltage)
  {
    dutyCycle += 1;
  }

  if (dutyCycle > 200)
  {
    dutyCycle = 200;
  }
  else if (dutyCycle < 0)
  {
    dutyCycle = 0;
  }

  if (dutyCycle > 100)
  {
    setPWM(TIM_CHANNEL_1, dutyCycle - 100);
    setPWM(TIM_CHANNEL_2, 100); // turns On the buck high side 100% of the time
  }
  else if (dutyCycle < 100)
  {
    setPWM(TIM_CHANNEL_1, 0); // turns On the boost high side 100% of the time
    setPWM(TIM_CHANNEL_2, dutyCycle);
  }
  else if (dutyCycle == 100)
  {
    setPWM(TIM_CHANNEL_1, 0); // turns On the boost high side 100% of the time
    setPWM(TIM_CHANNEL_2, 100); // turns On the buck high side 100% of the time
  }

  // print values
  SerialUSB.print("Voltage In: ");
  SerialUSB.print(voltageIn);
  SerialUSB.print(" V, Voltage Out: ");
  SerialUSB.print(voltageOut);
  SerialUSB.print(" V, Current In: ");
  SerialUSB.print(currentIn);
  SerialUSB.print(" mA, Current Out: ");
  SerialUSB.print(currentOut);
  SerialUSB.print(" mA");
  SerialUSB.print(" Duty Cycle: ");
  SerialUSB.print(dutyCycle);
  SerialUSB.println("%");

  //digitalWrite(PIN_LED, HIGH);
  // digitalWrite(PIN_Q5, HIGH);
  // digitalWrite(PIN_Q6, HIGH);
  //delay(100);
  //digitalWrite(PIN_LED, LOW);
  // digitalWrite(PIN_Q5, LOW);
  // digitalWrite(PIN_Q6, LOW);
  delay(10);
}