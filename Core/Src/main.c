/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body (High-level Orchestrator)
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
#include "sensors.h"
#include "power.h"
#include "controller.h"
#include "comms.h"
#include "settings.h"
#include "system_config.h"
#include "usbd_cdc_if.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
IWDG_HandleTypeDef hiwdg;

/* USER CODE BEGIN PV */
__attribute__((section(".noinit"))) uint32_t dfu_magic;
__attribute__((section(".noinit"))) uint32_t dfu_magic_inv;
#define DFU_MAGIC_VALUE 0xDEADBEEF
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_IWDG_Init(void);
/* USER CODE BEGIN PFP */
static void JumpToBootloader(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static void JumpToBootloader(void) {
    void (*SysMemBootJump)(void);

    /* 0. Disable interrupts */
    __disable_irq();

    /* 1. Remap System Memory to 0x00000000 */
    __HAL_RCC_SYSCFG_CLK_ENABLE();
    __HAL_SYSCFG_REMAPMEMORY_SYSTEMFLASH();

    /* 2. Set Stack Pointer from System Memory (0x1FFFC800 for F072) */
    /* On F072, System Memory starts at 0x1FFFC800 */
    /* The first word is the Stack Pointer */
    uint32_t pSysMem = 0x1FFFC800;
    __set_MSP(*(uint32_t *)pSysMem);

    /* 3. The second word is the Reset Vector */
    SysMemBootJump = (void (*)(void)) (*(uint32_t *)(pSysMem + 4));

    /* 4. Jump! */
    SysMemBootJump();
}
/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void)
{
  /* USER CODE BEGIN 1 */
  /* Check if we should jump to bootloader before any peripheral init */
  if (dfu_magic == DFU_MAGIC_VALUE && dfu_magic_inv == ~DFU_MAGIC_VALUE) {
      dfu_magic = 0;
      dfu_magic_inv = 0;
      JumpToBootloader();
  }
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* Configure the system clock */
  SystemClock_Config();

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
  MX_TIM6_Init();
  MX_IWDG_Init();

  /* USER CODE BEGIN 2 */
  SETTINGS_Init();
  SENSORS_Init();
  POWER_Init();
  CONTROLLER_Init();
  COMMS_Init();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    HAL_IWDG_Refresh(&hiwdg);
    COMMS_HandleCommands();

    if (SENSORS_IsBufferReady()) 
    {
      uint16_t offset = (SENSORS_IsBufferReady() == 2) ? (ADC_BUF_LEN / 2) : 0;
      SENSORS_ClearBufferReady();

      SENSORS_Process(offset);
      CONTROLLER_UpdateHighRate();
    }

    CONTROLLER_Task();

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief IWDG Initialization Function
  * @param None
  * @retval None
  */
static void MX_IWDG_Init(void)
{
  /* IWDG clock is LSI (approx 40kHz) */
  /* Timeout = (Prescaler * Reload) / LSI */
  /* Timeout = (64 * 625) / 40000 = 1.0 seconds */
  hiwdg.Instance = IWDG;
  hiwdg.Init.Prescaler = IWDG_PRESCALER_64;
  hiwdg.Init.Window = IWDG_WINDOW_DISABLE;
  hiwdg.Init.Reload = 625;
  if (HAL_IWDG_Init(&hiwdg) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
 * @brief System Clock Configuration
 * @retval int
 */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI48;
  RCC_OscInitStruct.HSI48State = RCC_HSI48_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

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
// needed for printf
int _write(int file, char *ptr, int len)
{
  static uint8_t rc = USBD_OK;
  uint32_t timeout = 0;

  do
  {
    rc = CDC_Transmit_FS((uint8_t *)ptr, len);
    if (++timeout > 10000) break; // Timeout to prevent blocking without host
  } while (USBD_BUSY == rc);

  if (USBD_FAIL == rc)
  {
    return 0;
  }
  return len;
}
/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
  }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
}
#endif /* USE_FULL_ASSERT */
