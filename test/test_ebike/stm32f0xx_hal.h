#ifndef __STM32F0xx_HAL_H
#define __STM32F0xx_HAL_H

#include <stdint.h>

// Dummy HAL defines
#define HAL_OK 0
typedef uint32_t HAL_StatusTypeDef;

typedef void* GPIO_TypeDef;
#define GPIO_PIN_RESET 0
#define GPIO_PIN_SET   1
#define GPIO_PIN_0     0
#define GPIO_PIN_1     1
#define GPIO_PIN_2     2
#define GPIO_PIN_3     3
#define GPIO_PIN_4     4
#define GPIO_PIN_5     5
#define GPIO_PIN_6     6
#define GPIO_PIN_7     7
#define GPIO_PIN_8     8
#define GPIO_PIN_9     9
#define GPIO_PIN_10    10
#define GPIO_PIN_11    11
#define GPIO_PIN_12    12
#define GPIO_PIN_13    13
#define GPIO_PIN_14    14
#define GPIO_PIN_15    15

#define GPIOA          ((GPIO_TypeDef)0)
#define GPIOB          ((GPIO_TypeDef)1)
#define GPIOC          ((GPIO_TypeDef)2)
#define GPIOD          ((GPIO_TypeDef)3)
#define GPIOF          ((GPIO_TypeDef)4)

#endif
