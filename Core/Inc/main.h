/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#ifndef NATIVE_TEST
#include "stm32f0xx_hal.h"
#else
#include <stdint.h>
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
void HAL_GPIO_WritePin(GPIO_TypeDef* port, uint16_t pin, uint8_t state);
void HAL_GPIO_TogglePin(GPIO_TypeDef* port, uint16_t pin);
uint8_t HAL_GPIO_ReadPin(GPIO_TypeDef* port, uint16_t pin);
#endif

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define usbDetect_Pin GPIO_PIN_13
#define usbDetect_GPIO_Port GPIOC
#define VsenseIn_Pin GPIO_PIN_1
#define VsenseIn_GPIO_Port GPIOA
#define IsenseIn_Pin GPIO_PIN_2
#define IsenseIn_GPIO_Port GPIOA
#define VsenseOut_Pin GPIO_PIN_3
#define VsenseOut_GPIO_Port GPIOA
#define IsenseOut_Pin GPIO_PIN_4
#define IsenseOut_GPIO_Port GPIOA
#define NTC100K_Pin GPIO_PIN_5
#define NTC100K_GPIO_Port GPIOA
#define boostLOWside_Pin GPIO_PIN_7
#define boostLOWside_GPIO_Port GPIOA
#define buckLOWside_Pin GPIO_PIN_0
#define buckLOWside_GPIO_Port GPIOB
#define fanControllPin_Pin GPIO_PIN_8
#define fanControllPin_GPIO_Port GPIOC
#define boostHIGHside_Pin GPIO_PIN_8
#define boostHIGHside_GPIO_Port GPIOA
#define buckHIGHside_Pin GPIO_PIN_9
#define buckHIGHside_GPIO_Port GPIOA
#define LED_Pin GPIO_PIN_10
#define LED_GPIO_Port GPIOC
#define chipSelect_Pin GPIO_PIN_2
#define chipSelect_GPIO_Port GPIOD
#define LCD_RST_Pin GPIO_PIN_12
#define LCD_RST_GPIO_Port GPIOB
#define LCD_DC_Pin GPIO_PIN_13
#define LCD_DC_GPIO_Port GPIOB
#define LCD_CE_Pin GPIO_PIN_14
#define LCD_CE_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
