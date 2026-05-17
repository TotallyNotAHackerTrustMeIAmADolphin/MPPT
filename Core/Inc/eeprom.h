/**
  ******************************************************************************
  * @file    EEPROM_Emulation/inc/eeprom.h 
  * @author  MCD Application Team
  * @brief   This file contains all the functions prototypes for the EEPROM 
  *          emulation firmware library.
  ******************************************************************************
  */ 

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __EEPROM_H
#define __EEPROM_H

/* Includes ------------------------------------------------------------------*/
#include "stm32f0xx_hal.h"

/* Exported constants --------------------------------------------------------*/
/* EEPROM start address in Flash */
#define EEPROM_START_ADDRESS    ((uint32_t)0x0801F000) /* Page 62 */

/* Pages 0 and 1 base and end addresses */
#define PAGE0_BASE_ADDRESS      ((uint32_t)(EEPROM_START_ADDRESS + 0x0000))
#define PAGE0_END_ADDRESS       ((uint32_t)(EEPROM_START_ADDRESS + (FLASH_PAGE_SIZE - 1)))
#define PAGE0_ID                FLASH_PAGE_62

#define PAGE1_BASE_ADDRESS      ((uint32_t)(EEPROM_START_ADDRESS + FLASH_PAGE_SIZE))
#define PAGE1_END_ADDRESS       ((uint32_t)(EEPROM_START_ADDRESS + (2 * FLASH_PAGE_SIZE - 1)))
#define PAGE1_ID                FLASH_PAGE_63

/* Used Flash pages for EEPROM emulation */
#define PAGE0                   ((uint16_t)0x0000)
#define PAGE1                   ((uint16_t)0x0001)

/* No valid page define */
#define NO_VALID_PAGE           ((uint16_t)0x00AB)

/* Page status definitions */
#define ERASED                  ((uint16_t)0xFFFF)     /* Page is empty */
#define RECEIVE_DATA            ((uint16_t)0xEEEE)     /* Page is marked to receive data */
#define VALID_PAGE              ((uint16_t)0x0000)     /* Page containing valid data */

/* Valid pages definitions */
#define VALID_PAGE0             ((uint16_t)0x0000)
#define VALID_PAGE1             ((uint16_t)0x0001)

/* Cycle state definitions */
#define ERASE_COMPLETE          ((uint16_t)0x0000)
#define ERASE_IN_PROGRESS       ((uint16_t)0x0001)

/* EEPROM variable default value */
#define EE_DEFAULT_VALUE        ((uint16_t)0x0000)

/* Exported types ------------------------------------------------------------*/
/* Exported macro ------------------------------------------------------------*/
/* Exported functions ------------------------------------------------------- */
uint16_t EE_Init(void);
uint16_t EE_ReadVariable(uint16_t VirtAddress, uint16_t* Data);
uint16_t EE_WriteVariable(uint16_t VirtAddress, uint16_t Data);

/* Virtual Address Definitions for Calibration constants */
#define VIRT_VOLT_IN_SLOPE      ((uint16_t)0x0001)
#define VIRT_VOLT_IN_OFFSET     ((uint16_t)0x0002)
#define VIRT_VOLT_OUT_SLOPE     ((uint16_t)0x0003)
#define VIRT_VOLT_OUT_OFFSET    ((uint16_t)0x0004)
#define VIRT_AMP_IN_SLOPE       ((uint16_t)0x0005)
#define VIRT_AMP_IN_OFFSET      ((uint16_t)0x0006)
#define VIRT_AMP_OUT_SLOPE      ((uint16_t)0x0007)
#define VIRT_AMP_OUT_OFFSET     ((uint16_t)0x0008)

#endif /* __EEPROM_H */
