/**
  ******************************************************************************
  * @file    EEPROM_Emulation/src/eeprom.c 
  * @author  MCD Application Team
  * @brief   This file provides all the EEPROM emulation firmware functions.
  ******************************************************************************
  */ 

/* Includes ------------------------------------------------------------------*/
#include "eeprom.h"

/* Private function prototypes -----------------------------------------------*/
static HAL_StatusTypeDef EE_Format(void);
static uint16_t EE_FindValidPage(uint8_t Operation);
static uint16_t EE_VerifyPageFullWriteVariable(uint16_t VirtAddress, uint16_t Data);
static uint16_t EE_PageTransfer(uint16_t VirtAddress, uint16_t Data);

/**
  * @brief  Restore the pages to a known good state in case of page's status
  *   corruption after several power down.
  * @param  None
  * @retval - Flash error code: on write Flash error
  *         - FLASH_COMPLETE: on success
  */
uint16_t EE_Init(void)
{
  uint16_t PageStatus0 = 6, PageStatus1 = 6;
  HAL_StatusTypeDef FlashStatus;

  /* Get Page0 status */
  PageStatus0 = (*(__IO uint16_t*)PAGE0_BASE_ADDRESS);
  /* Get Page1 status */
  PageStatus1 = (*(__IO uint16_t*)PAGE1_BASE_ADDRESS);

  /* Check for invalid header states and repair if necessary */  
  switch (PageStatus0)
  {
    case ERASED:
      if (PageStatus1 == VALID_PAGE) { /* Page0 erased, Page1 valid */
          return HAL_OK;
      }
      else if (PageStatus1 == RECEIVE_DATA) { /* Page0 erased, Page1 receive */
          FlashStatus = HAL_FLASH_Unlock();
          FlashStatus = HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, PAGE1_BASE_ADDRESS, VALID_PAGE);
          HAL_FLASH_Lock();
          return FlashStatus;
      }
      else { /* First boot or both erased */
          return EE_Format();
      }
      break;

    case RECEIVE_DATA:
      if (PageStatus1 == VALID_PAGE) { /* Page0 receive, Page1 valid */
          /* Transfer data from Page1 to Page0 */
          return EE_PageTransfer(0, 0xFFFF);
      }
      else if (PageStatus1 == ERASED) { /* Page0 receive, Page1 erased */
          FlashStatus = HAL_FLASH_Unlock();
          FlashStatus = HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, PAGE0_BASE_ADDRESS, VALID_PAGE);
          HAL_FLASH_Lock();
          return FlashStatus;
      }
      else {
          return EE_Format();
      }
      break;

    case VALID_PAGE:
      if (PageStatus1 == VALID_PAGE) { /* Both pages valid, error */
          return EE_Format();
      }
      else if (PageStatus1 == ERASED) { /* Page0 valid, Page1 erased */
          return HAL_OK;
      }
      else { /* Page0 valid, Page1 receive */
          /* Transfer data from Page0 to Page1 */
          return EE_PageTransfer(0, 0xFFFF);
      }
      break;

    default:  /* Any other state -> format */
      return EE_Format();
      break;
  }
}

/**
  * @brief  Returns the last stored variable data, if found, which correspond to
  *   the passed virtual address
  * @param  VirtAddress: Variable virtual address
  * @param  Data: Global variable where the read variable value will be stored
  * @retval - Success or error status:
  *           - 0: if variable was found
  *           - 1: if the variable was not found
  *           - NO_VALID_PAGE: if no valid page was found.
  */
uint16_t EE_ReadVariable(uint16_t VirtAddress, uint16_t* Data)
{
  uint16_t ValidPage = PAGE0;
  uint32_t Address = 0x08010000, PageStartAddress = 0x08010000;

  /* Get active Page for read operation */
  ValidPage = EE_FindValidPage(0);

  /* Check if there is no valid page */
  if (ValidPage == NO_VALID_PAGE)
  {
    return  NO_VALID_PAGE;
  }

  /* Get the valid Page start Address */
  PageStartAddress = (uint32_t)(EEPROM_START_ADDRESS + (uint32_t)(ValidPage * FLASH_PAGE_SIZE));

  /* Get the valid Page end Address */
  Address = (uint32_t)((PageStartAddress + FLASH_PAGE_SIZE) - 2);

  /* Check each offset address from end of page to look for the virtual address */
  while (Address > (PageStartAddress + 2))
  {
    /* Get the current location content to be compared with virtual address */
    if ((*(__IO uint16_t*)Address) == VirtAddress)
    {
      /* Get the variable value */
      *Data = (*(__IO uint16_t*)(Address - 2));
      return 0;
    }
    else
    {
      Address = Address - 4;
    }
  }

  /* Return 1 value if variable not found */
  return 1;
}

/**
  * @brief  Writes/updates variable data in EEPROM.
  * @param  VirtAddress: Variable virtual address
  * @param  Data: 16 bit data to be written
  * @retval - Success or error status:
  *           - FLASH_COMPLETE: on success
  *           - PAGE_FULL: if valid page is full
  *           - NO_VALID_PAGE: if no valid page was found
  *           - Flash error code: on write Flash error
  */
uint16_t EE_WriteVariable(uint16_t VirtAddress, uint16_t Data)
{
  uint16_t Status = 0;

  /* Write the variable virtual address and value in the EEPROM */
  Status = EE_VerifyPageFullWriteVariable(VirtAddress, Data);

  /* In case the line is full, transfer to other page */
  if (Status == 1)
  {
    Status = EE_PageTransfer(VirtAddress, Data);
  }

  /* Return last operation status */
  return Status;
}

/**
  * @brief  Erases PAGE0 and PAGE1 and writes VALID_PAGE header to PAGE0
  * @param  None
  * @retval Status of the last operation (Flash write or erase) code
  */
static HAL_StatusTypeDef EE_Format(void)
{
  HAL_StatusTypeDef FlashStatus = HAL_OK;
  uint32_t PageError = 0;
  FLASH_EraseInitTypeDef s_eraseinit;

  s_eraseinit.TypeErase = FLASH_TYPEERASE_PAGES;
  s_eraseinit.PageAddress = PAGE0_BASE_ADDRESS;
  s_eraseinit.NbPages = 2;

  FlashStatus = HAL_FLASH_Unlock();
  if (FlashStatus == HAL_OK)
  {
    FlashStatus = HAL_FLASHEx_Erase(&s_eraseinit, &PageError);
    if (FlashStatus == HAL_OK)
    {
      FlashStatus = HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, PAGE0_BASE_ADDRESS, VALID_PAGE);
    }
  }
  HAL_FLASH_Lock();

  return FlashStatus;
}

/**
  * @brief  Find valid page for write or read operation
  * @param  Operation: operation to be performed (0 for read, 1 for write)
  * @retval Valid page index (PAGE0 or PAGE1) or NO_VALID_PAGE if no valid page was found
  */
static uint16_t EE_FindValidPage(uint8_t Operation)
{
  uint16_t PageStatus0 = 6, PageStatus1 = 6;

  /* Get Page0 status */
  PageStatus0 = (*(__IO uint16_t*)PAGE0_BASE_ADDRESS);
  /* Get Page1 status */
  PageStatus1 = (*(__IO uint16_t*)PAGE1_BASE_ADDRESS);

  /* Write operation */
  if (Operation == 1)
  {
    if (PageStatus1 == VALID_PAGE) return PAGE1;
    if (PageStatus0 == VALID_PAGE) return PAGE0;
    return NO_VALID_PAGE;
  }
  /* Read operation */
  else
  {
    if (PageStatus0 == VALID_PAGE) return PAGE0;
    if (PageStatus1 == VALID_PAGE) return PAGE1;
    return NO_VALID_PAGE;
  }
}

/**
  * @brief  Verify if active page is full and writes variable in it.
  * @param  VirtAddress: Variable virtual address
  * @param  Data: 16 bit data to be written
  * @retval - Success or error status:
  *           - FLASH_COMPLETE: on success
  *           - 1: if the page is full
  *           - NO_VALID_PAGE: if no valid page was found
  *           - Flash error code: on write Flash error
  */
static uint16_t EE_VerifyPageFullWriteVariable(uint16_t VirtAddress, uint16_t Data)
{
  HAL_StatusTypeDef FlashStatus = HAL_OK;
  uint16_t ValidPage = PAGE0;
  uint32_t Address = 0x08010000, PageEndAddress = 0x08010000;

  /* Get active Page for write operation */
  ValidPage = EE_FindValidPage(1);

  if (ValidPage == NO_VALID_PAGE) return  NO_VALID_PAGE;

  /* Get the valid Page start Address */
  Address = (uint32_t)(EEPROM_START_ADDRESS + (uint32_t)(ValidPage * FLASH_PAGE_SIZE));

  /* Get the valid Page end Address */
  PageEndAddress = (uint32_t)((EEPROM_START_ADDRESS + (uint32_t)((ValidPage + 1) * FLASH_PAGE_SIZE)) - 2);

  /* Check each offset address from beginning of page to look for an empty location */
  while (Address < PageEndAddress)
  {
    if ((*(__IO uint32_t*)Address) == 0xFFFFFFFF)
    {
      /* Set variable data and virtual address */
      FlashStatus = HAL_FLASH_Unlock();
      if (FlashStatus == HAL_OK) {
          FlashStatus = HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, Address, Data);
          if (FlashStatus == HAL_OK) {
              FlashStatus = HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, Address + 2, VirtAddress);
          }
      }
      HAL_FLASH_Lock();
      return FlashStatus;
    }
    else
    {
      Address = Address + 4;
    }
  }

  /* Page full */
  return 1;
}

/**
  * @brief  Transfers last updated variables data from the full Page to an empty one.
  * @param  VirtAddress: Variable virtual address
  * @param  Data: 16 bit data to be written
  * @retval - Success or error status:
  *           - FLASH_COMPLETE: on success
  *           - Flash error code: on write Flash error
  */
static uint16_t EE_PageTransfer(uint16_t VirtAddress, uint16_t Data)
{
  HAL_StatusTypeDef FlashStatus = HAL_OK;
  uint32_t NewPageAddress = 0x08010000, OldPageAddress = 0x08010000;
  uint16_t ValidPage = PAGE0;
  uint32_t PageError = 0;
  FLASH_EraseInitTypeDef s_eraseinit;

  /* Get active Page for transfer operation */
  ValidPage = EE_FindValidPage(1);

  if (ValidPage == PAGE1) {
    NewPageAddress = PAGE0_BASE_ADDRESS;
    OldPageAddress = PAGE1_BASE_ADDRESS;
  }
  else if (ValidPage == PAGE0) {
    NewPageAddress = PAGE1_BASE_ADDRESS;
    OldPageAddress = PAGE0_BASE_ADDRESS;
  }
  else return NO_VALID_PAGE;

  /* Set new Page status to RECEIVE_DATA */
  FlashStatus = HAL_FLASH_Unlock();
  FlashStatus = HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, NewPageAddress, RECEIVE_DATA);
  if (FlashStatus != HAL_OK) return FlashStatus;

  /* Write the variable passed as parameter in the new page */
  if (VirtAddress != 0) {
      FlashStatus = EE_VerifyPageFullWriteVariable(VirtAddress, Data);
      if (FlashStatus != HAL_OK) return FlashStatus;
  }

  /* Transfer all other variables */
  /* For this project, we only have 8 variables. We can just iterate them. */
  for (uint16_t var = 1; var <= 8; var++) {
      if (var != VirtAddress) {
          uint16_t read_val = 0;
          if (EE_ReadVariable(var, &read_val) == 0) {
              EE_VerifyPageFullWriteVariable(var, read_val);
          }
      }
  }

  /* Erase old page */
  s_eraseinit.TypeErase = FLASH_TYPEERASE_PAGES;
  s_eraseinit.PageAddress = OldPageAddress;
  s_eraseinit.NbPages = 1;
  FlashStatus = HAL_FLASHEx_Erase(&s_eraseinit, &PageError);
  if (FlashStatus != HAL_OK) return FlashStatus;

  /* Set new Page status to VALID_PAGE */
  FlashStatus = HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, NewPageAddress, VALID_PAGE);
  HAL_FLASH_Lock();

  return FlashStatus;
}
