#include "BuckBoost.h"

void BuckBoost::HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    if (htim->Instance == TIM1)
    {
        __HAL_RCC_GPIOA_CLK_ENABLE();
        __HAL_RCC_GPIOB_CLK_ENABLE();

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
    }
}

void BuckBoost::MX_TIM1_Init()
{
    TIM_ClockConfigTypeDef sClockSourceConfig = {0};
    TIM_MasterConfigTypeDef sMasterConfig = {0};
    TIM_OC_InitTypeDef sConfigOC = {0};
    TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};

    htim1.Instance = TIM1;
    htim1.Init.Prescaler = 0;
    htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim1.Init.Period = TIMER_PERIOD;
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
    HAL_TIM_MspPostInit(&htim1);
}

void BuckBoost::MX_DMA_Init()
{
    __HAL_RCC_DMA1_CLK_ENABLE();
    HAL_NVIC_SetPriority(DMA1_Channel2_3_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(DMA1_Channel2_3_IRQn);
}

void BuckBoost::MX_GPIO_Init()
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
}

void BuckBoost::updateDitherTable(uint16_t *pDitherTable, uint16_t desiredDutyCycle)
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

void BuckBoost::setPWM(float dutyCyclePct)
{
    uint16_t value = 0;

    // constrain input to 0-200%
    if (dutyCyclePct < -1)
    {
        dutyCyclePct = -1;
    }
    else if (dutyCyclePct > 200)
    {
        dutyCyclePct = 200;
    }

    if (dutyCyclePct < 0)
    {
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0);
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, 0);
    }
    else if (dutyCyclePct < 100)
    {
        value = (uint16_t)roundf(dutyCyclePct / 100 * (float)(TIMER_PERIOD + 1));
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, value);            // set the duty cycle for the buck converter
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, TIMER_PERIOD + 1); // set the duty cycle of the boost converter to 100%
    }
    else if (dutyCyclePct == 100)
    {
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, TIMER_PERIOD + 1); // set the duty cycle of the boost converter to 100%
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, TIMER_PERIOD + 1); // set the duty cycle of the buck converter to 100%
    }
    else if (dutyCyclePct > 100)
    {
        value = (uint16_t)roundf((dutyCyclePct - 100) / 100 * (float)(TIMER_PERIOD + 1));
        value = map(value, 0, TIMER_PERIOD + 1, TIMER_PERIOD + 1, 0);   // invert the value due to topologie
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, value);            // set the duty cycle for the boost converter
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, TIMER_PERIOD + 1); // set the duty cycle of the buck converter to 100%
    }
}

void BuckBoost::measure()
{
    float temp = 0;

    static float previousmAmpIn = 0;
    temp = analogRead(PIN_A2); // mA
    temp -= 2048;              // 12 bit ADC
    temp *= CURRENT_CONVERSION_FACTOR;
    mAmpIn = temp * 0.1 + previousmAmpIn * 0.9;
    previousmAmpIn = mAmpIn;

    static float previousmAmpOut = 0;
    temp = analogRead(PIN_A4); // mA
    temp -= 2048;
    temp *= CURRENT_CONVERSION_FACTOR;
    mAmpOut = temp * 0.1 + previousmAmpOut * 0.9;
    previousmAmpOut = mAmpOut;

    static float previousVoltIn = 0;
    temp = analogRead(PIN_A1);
    temp *= VOLTAGE_CONVERSION_FACTOR;
    voltIn = temp * 0.1 + previousVoltIn * 0.9;
    previousVoltIn = voltIn;

    static float previousVoltOut = 0;
    temp = analogRead(PIN_A3);
    temp *= VOLTAGE_CONVERSION_FACTOR;
    voltOut = temp * 0.1 + previousVoltOut * 0.9;
    previousVoltOut = voltOut;
}

void BuckBoost::initPWM()
{
    MX_GPIO_Init();
    MX_DMA_Init();
    MX_TIM1_Init();

    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);

    setPWM(-1);
}

void BuckBoost::init()
{
    initPWM();
}

void BuckBoost::run()
{
    static uint16_t previousTime = 0;
    uint16_t currentTime = millis();
    if (currentTime - previousTime >= 100)
    {
        previousTime = currentTime;
        measure();
    }
}
