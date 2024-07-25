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
#include "stm32f0xx_hal.h"

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

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
