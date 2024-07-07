#ifndef BUCKBOOST_H
#define BUCKBOOST_H

#include <Arduino.h>
#include <constants.h>

#define TIMER_PERIOD ((uint16_t)479) // 100 kHz PWM frequency
#define DITHER_TABLE_SIZE 8          // 3 bits of dithering, has to be a power of 2

class BuckBoost
{
public:
    void init();
    void run();

    float mAmpIn;
    float mAmpOut;

    float voltIn;
    float voltOut;

private:
    void SystemClock_Config();
    void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);
    void MX_TIM1_Init();
    void MX_DMA_Init();
    void MX_GPIO_Init();
    void updateDitherTable(uint16_t *pDitherTable, uint16_t desiredDutyCycle);
    void setPWM(float dutyCyclePct);
    void initPWM();
    void measure();

    TIM_HandleTypeDef htim1;
    DMA_HandleTypeDef hdma_tim1_ch1;
    DMA_HandleTypeDef hdma_tim1_ch2;

    uint16_t ditherTableCH1[2 * DITHER_TABLE_SIZE];
    uint16_t ditherTableCH2[2 * DITHER_TABLE_SIZE];
    uint16_t maxDutyCycle;
    uint8_t ditherBits;

    float dutyCycle;
    float targetVoltage;
};

#endif // BUCKBOOST_H
