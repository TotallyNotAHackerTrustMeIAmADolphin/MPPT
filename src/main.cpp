#include <Arduino.h>
#include <main.c>

void setup()
{
  initPWM();
  setPWM(TIM_CHANNEL_1, 0);
  setPWM(TIM_CHANNEL_2, 0);
}

void loop()
{

}