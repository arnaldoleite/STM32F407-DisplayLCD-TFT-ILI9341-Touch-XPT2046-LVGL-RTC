/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "display/ili9341.h"
#include "display/image.h"
#include "lvgl.h"
#include "lv_port_indev.h"
#include "lv_port_disp.h"
#include "ui.h"
#include "touch/xpt2046.h"
#include "serialmemory/w25qxx.h"
#include <stdio.h>
#include <string.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define PAGE_SIZE 4096
#define RUN_TESTS 1



/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
RTC_HandleTypeDef hrtc;

SPI_HandleTypeDef hspi2;
SPI_HandleTypeDef hspi3;

SRAM_HandleTypeDef hsram1;

/* USER CODE BEGIN PV */

W25QXX_HandleTypeDef w25qxx; // Handler for all w25qxx operations!
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_FSMC_Init(void);
static void MX_SPI2_Init(void);
static void MX_RTC_Init(void);
static void MX_SPI3_Init(void);
/* USER CODE BEGIN PFP */
static void serialmempory_init(void);
static void rtc_set_datetime(void);
static void rtc_lvgl_timer_cb(lv_timer_t *timer);

void runTests(void);
void dump_hex(char *header, uint32_t start, uint8_t *buf, uint32_t len);
void fill_buffer(uint8_t pattern, uint8_t *buf, uint32_t len);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
// Dump hex to serial console
void dump_hex(char *header, uint32_t start, uint8_t *buf, uint32_t len) {
    uint32_t i = 0;

    printf("%s\n", header);

    for (i = 0; i < len; ++i) {

        if (i % 16 == 0) {
            printf("0x%08lx: ", start);
        }

        printf("%02x ", buf[i]);

        if ((i + 1) % 16 == 0) {
            printf("\n");
        }

        ++start;
    }
}

void fill_buffer(uint8_t pattern, uint8_t *buf, uint32_t len) {
    switch (pattern) {
    case 0:
        memset(buf, 0, len);
        break;
    case 1:
        memset(buf, 0xaa, len); // 10101010
        break;
    case 2:
        for (uint32_t i = 0; i < len; ++i)
            buf[i] = i % 256;
        break;
    default:
        DBG("Programmer is a moron");
    }
}

uint8_t check_buffer(uint8_t pattern, uint8_t *buf, uint32_t len) {

    uint8_t ret = 1;

    switch (pattern) {
    case 0:
        for (uint32_t i = 0; i < len; ++i) {
            if (buf[i] != 0)
                ret = 0;
        }
        break;
    case 1:
        for (uint32_t i = 0; i < len; ++i) {
            if (buf[i] != 0xaa)
                ret = 0;
        }
        break;
    case 2:
        for (uint32_t i = 0; i < len; ++i) {
            if (buf[i] != i % 256)
                ret = 0;
        }
        break;
    default:
        DBG("Programmer is a moron");
    }

    return ret;
}

uint32_t get_sum(uint8_t *buf, uint32_t len) {
    uint32_t sum = 0;
    for (uint32_t i = 0; i < len; ++i) {
        sum += buf[i];
    }
    return sum;
}
void runTests(void) {
	  W25QXX_result_t res;

		HAL_Delay(2000);

		uint8_t buf[PAGE_SIZE]; // Buffer the size of a page

		for (uint8_t run = 0; run <= 2; ++run) {

			DBG("\n-------------\nRun %d\n", run);

			DBG("Reading first page");

			res = w25qxx_read(&w25qxx, 0, (uint8_t*) &buf, sizeof(buf));
			if (res == W25QXX_Ok) {
				dump_hex("First page at start", 0, (uint8_t*) &buf, sizeof(buf));
			} else {
				DBG("Unable to read w25qxx\n");
			}

			DBG("Erasing first page");
			if (w25qxx_erase(&w25qxx, 0, sizeof(buf)) == W25QXX_Ok) {
				DBG("Reading first page\n");
				if (w25qxx_read(&w25qxx, 0, (uint8_t*) &buf, sizeof(buf)) == W25QXX_Ok) {
					dump_hex("After erase", 0, (uint8_t*) &buf, sizeof(buf));
				}
			}

			// Create a well known pattern
			fill_buffer(run, buf, sizeof(buf));

			// Write it to device
			DBG("Writing first page\n");
			if (w25qxx_write(&w25qxx, 0, (uint8_t*) &buf, sizeof(buf)) == W25QXX_Ok) {
				// now read it back
				DBG("Reading first page\n");
				if (w25qxx_read(&w25qxx, 0, (uint8_t*) &buf, sizeof(buf)) == W25QXX_Ok) {
					//DBG("  - sum = %lu", get_sum(buf, 256));
					dump_hex("After write", 0, (uint8_t*) &buf, sizeof(buf));
				}
			}
		}

		// Let's do a stress test
		uint32_t start;
		uint32_t sectors = w25qxx.block_count * w25qxx.sectors_in_block; // Entire chip

		DBG("Stress testing w25qxx device: sectors = %lu\n", sectors);

		DBG("Doing chip erase\n");
		start = HAL_GetTick();
		w25qxx_chip_erase(&w25qxx);
		DBG("Done erasing - took %lu ms\n", HAL_GetTick() - start);

		fill_buffer(0, buf, sizeof(buf));

		DBG("Writing all zeroes %lu sectors\n", sectors);
		start = HAL_GetTick();
		for (uint32_t i = 0; i < sectors; ++i) {
			w25qxx_write(&w25qxx, i * w25qxx.sector_size, buf, sizeof(buf));
		}
		DBG("Done writing - took %lu ms\n", HAL_GetTick() - start);

		DBG("Reading %lu sectors\n", sectors);
		start = HAL_GetTick();
		for (uint32_t i = 0; i < sectors; ++i) {
			w25qxx_read(&w25qxx, i * w25qxx.sector_size, buf, sizeof(buf));
		}
		DBG("Done reading - took %lu ms\n", HAL_GetTick() - start);

		DBG("Validating buffer .... ");
		if (check_buffer(0, buf, sizeof(buf))) {
			DBG("OK\n");
		} else {
			DBG("Not OK\n");
		}

		DBG("Doing chip erase\n");
		start = HAL_GetTick();
		w25qxx_chip_erase(&w25qxx);
		DBG("Done erasing - took %lu ms\n", HAL_GetTick() - start);

		fill_buffer(1, buf, sizeof(buf));

		DBG("Writing 10101010 (0xaa) %lu sectors\n", sectors);
		start = HAL_GetTick();
		for (uint32_t i = 0; i < sectors; ++i) {
			w25qxx_write(&w25qxx, i * w25qxx.sector_size, buf, sizeof(buf));
		}
		DBG("Done writing - took %lu ms\n", HAL_GetTick() - start);

		DBG("Reading %lu sectors\n", sectors);
		start = HAL_GetTick();
		for (uint32_t i = 0; i < sectors; ++i) {
			w25qxx_read(&w25qxx, i * w25qxx.sector_size, buf, sizeof(buf));
		}
		DBG("Done reading - took %lu ms\n", HAL_GetTick() - start);

		DBG("Validating buffer ... ");
		if (check_buffer(1, buf, sizeof(buf))) {
			DBG("OK\n");
		} else {
			DBG("Not OK\n");
		}

		DBG("Erasing %lu sectors sequentially\n", sectors);
		start = HAL_GetTick();
		for (uint32_t i = 0; i < sectors; ++i) {
			w25qxx_erase(&w25qxx, i * w25qxx.sector_size, sizeof(buf));
			if ((i > 0) && (i % 100 == 0)) {
				DBG("Done %4lu sectors - total time = %3lu s\n", i, (HAL_GetTick() - start) / 1000);
			}
		}
		DBG("Done erasing - took %lu ms\n", HAL_GetTick() - start);

}
static void serialmempory_initr(void)
{
	char readBuffer[PAGE_SIZE] = {0x00};
	  W25QXX_result_t res;

	  res = w25qxx_init(&w25qxx, &hspi3, SPI3_CS_GPIO_Port, SPI3_CS_Pin);
	  if (res == W25QXX_Ok) {
		  DBG("W25QXX successfully initialized\n");
		  DBG("Manufacturer       = 0x%2x\n", w25qxx.manufacturer_id);
		  DBG("Device             = 0x%4x\n", w25qxx.device_id);
		  DBG("Block size         = 0x%04lx (%lu)\n", w25qxx.block_size, w25qxx.block_size);
		  DBG("Block count        = 0x%04lx (%lu)\n", w25qxx.block_count, w25qxx.block_count);
		  DBG("Sector size        = 0x%04lx (%lu)\n", w25qxx.sector_size, w25qxx.sector_size);
		  DBG("Sectors per block  = 0x%04lx (%lu)\n", w25qxx.sectors_in_block, w25qxx.sectors_in_block);
		  DBG("Page size          = 0x%04lx (%lu)\n", w25qxx.page_size, w25qxx.page_size);
		  DBG("Pages per sector   = 0x%04lx (%lu)\n", w25qxx.pages_in_sector, w25qxx.pages_in_sector);
		  DBG("Total size (in kB) = 0x%04lx (%lu)\n",
				  (w25qxx.block_count * w25qxx.block_size) / 1024, (w25qxx.block_count * w25qxx.block_size) / 1024);
		  DBG("Total size (in Bytes) = 0x%04lx (%lu)\n",
				  (w25qxx.block_count * w25qxx.block_size), (w25qxx.block_count * w25qxx.block_size));
	  } else {
		  DBG("Unable to initialize w25qxx\n");
	  }

	#ifdef RUN_TESTS
	  runTests();
	#else
	#define PARAGRAPH small_paragraph

	  int len = strlen(PARAGRAPH);

	  res = w25qxx_write(&w25qxx, (uint32_t) 0x02321, (uint8_t *) PARAGRAPH, len);

	  if (res != W25QXX_Ok) {
		  Error_Handler();
	  } else {
		  printf("No Errors! Completed writing %d bytes of small paragraph!\r\n", len);
	  }

	  res = w25qxx_read(&w25qxx, (uint32_t) 0x02321, (uint8_t *) &readBuffer, len);

	  if (res != W25QXX_Ok) {
		  Error_Handler();
	  } else {
		  printf("No Errors! Completed reading %d bytes of memory containing small paragraph!\r\n\r\n", len);
		  printf(readBuffer);
		  fflush(stdout);
	  }
	#endif
}
static void rtc_set_datetime()
{
	RTC_TimeTypeDef sTime = {0};
	RTC_DateTypeDef sDate = {0};


	sTime.Hours = 13;
	sTime.Minutes = 33;
	sTime.Seconds = 32;
	sTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
	sTime.StoreOperation = RTC_STOREOPERATION_RESET;
	if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN) != HAL_OK)
	{
		Error_Handler();
	}


	sDate.WeekDay = RTC_WEEKDAY_FRIDAY;
	sDate.Month = RTC_MONTH_JANUARY;
	sDate.Date = 16;
	sDate.Year = 26;

	if (HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BIN) != HAL_OK)
	{
		Error_Handler();
	}
}
static void rtc_lvgl_timer_cb(lv_timer_t *timer)
{
    RTC_TimeTypeDef time;
    RTC_DateTypeDef date;
    char buf[32];

    /* OBRIGATÓRIO: primeiro Time, depois Date */
    HAL_RTC_GetTime(&hrtc, &time, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &date, RTC_FORMAT_BIN);

    snprintf(buf, sizeof(buf),
             "%02d/%02d/20%02d  %02d:%02d:%02d",
             date.Date,
             date.Month,
             date.Year,
             time.Hours,
             time.Minutes,
             time.Seconds);

    lv_label_set_text(objects.lb_name, buf);
}
void action_cmd_main_clicked(lv_event_t * event)
{
	lv_label_set_text(objects.lb_name,  "Clicked");
	printf("Clicked\n");
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
  uint16_t x = 0, y = 0;
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_FSMC_Init();
  MX_SPI2_Init();
  MX_RTC_Init();
  MX_SPI3_Init();
  /* USER CODE BEGIN 2 */
  /* Turn ON backlight (PB1) */
  LCD_BL_ON();

  lcdInit();
  lcdSetOrientation(LCD_ORIENTATION_LANDSCAPE);

  lv_init();
  lv_tick_set_cb(HAL_GetTick);

  lv_port_disp_init();
  lv_port_indev_init();

  lv_timer_create(rtc_lvgl_timer_cb, 1000, NULL);
  //Set date time here the first time
  //rtc_set_datetime();

  ui_init();
  //lcdTest();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

	//lv_label_set_text(objects.lb_name,  "STM32 LVGL EEZ Studio");
	//lv_slider_set_value(objects.slidder_main,100,LV_ANIM_OFF);
	uint32_t time_till_next = lv_timer_handler();
	//if(time_till_next == LV_NO_TIMER_READY) time_till_next = LV_DEF_REFR_PERIOD; /*handle LV_NO_TIMER_READY. Another option is to `sleep` for longer*/
	HAL_Delay(time_till_next);



  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_LSE;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 64;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief RTC Initialization Function
  * @param None
  * @retval None
  */
static void MX_RTC_Init(void)
{

  /* USER CODE BEGIN RTC_Init 0 */

  /* USER CODE END RTC_Init 0 */

  RTC_TimeTypeDef sTime = {0};
  RTC_DateTypeDef sDate = {0};

  /* USER CODE BEGIN RTC_Init 1 */

  /* USER CODE END RTC_Init 1 */

  /** Initialize RTC Only
  */
  hrtc.Instance = RTC;
  hrtc.Init.HourFormat = RTC_HOURFORMAT_24;
  hrtc.Init.AsynchPrediv = 127;
  hrtc.Init.SynchPrediv = 255;
  hrtc.Init.OutPut = RTC_OUTPUT_DISABLE;
  hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
  hrtc.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;
  if (HAL_RTC_Init(&hrtc) != HAL_OK)
  {
    Error_Handler();
  }

  /* USER CODE BEGIN Check_RTC_BKUP */

  /* USER CODE END Check_RTC_BKUP */

  /** Initialize RTC and set the Time and Date
  */
  sTime.Hours = 0x0;
  sTime.Minutes = 0x0;
  sTime.Seconds = 0x0;
  sTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
  sTime.StoreOperation = RTC_STOREOPERATION_RESET;
  if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BCD) != HAL_OK)
  {
    Error_Handler();
  }
  sDate.WeekDay = RTC_WEEKDAY_MONDAY;
  sDate.Month = RTC_MONTH_JANUARY;
  sDate.Date = 0x1;
  sDate.Year = 0x0;

  if (HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BCD) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN RTC_Init 2 */

  /* USER CODE END RTC_Init 2 */

}

/**
  * @brief SPI2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI2_Init(void)
{

  /* USER CODE BEGIN SPI2_Init 0 */

  /* USER CODE END SPI2_Init 0 */

  /* USER CODE BEGIN SPI2_Init 1 */

  /* USER CODE END SPI2_Init 1 */
  /* SPI2 parameter configuration*/
  hspi2.Instance = SPI2;
  hspi2.Init.Mode = SPI_MODE_MASTER;
  hspi2.Init.Direction = SPI_DIRECTION_2LINES;
  hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi2.Init.NSS = SPI_NSS_SOFT;
  hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
  hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi2.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI2_Init 2 */

  /* USER CODE END SPI2_Init 2 */

}

/**
  * @brief SPI3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI3_Init(void)
{

  /* USER CODE BEGIN SPI3_Init 0 */

  /* USER CODE END SPI3_Init 0 */

  /* USER CODE BEGIN SPI3_Init 1 */

  /* USER CODE END SPI3_Init 1 */
  /* SPI3 parameter configuration*/
  hspi3.Instance = SPI3;
  hspi3.Init.Mode = SPI_MODE_MASTER;
  hspi3.Init.Direction = SPI_DIRECTION_2LINES;
  hspi3.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi3.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi3.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi3.Init.NSS = SPI_NSS_SOFT;
  hspi3.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
  hspi3.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi3.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi3.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi3.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI3_Init 2 */

  /* USER CODE END SPI3_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12|GPIO_PIN_6, GPIO_PIN_RESET);

  /*Configure GPIO pin : PC5 */
  GPIO_InitStruct.Pin = GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : PB12 PB6 */
  GPIO_InitStruct.Pin = GPIO_PIN_12|GPIO_PIN_6;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */
  //GPIO_InitTypeDef GPIO_InitStruct = {0};
  GPIO_InitStruct.Pin = GPIO_PIN_1;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
  /* USER CODE END MX_GPIO_Init_2 */
}

/* FSMC initialization function */
static void MX_FSMC_Init(void)
{

  /* USER CODE BEGIN FSMC_Init 0 */

  /* USER CODE END FSMC_Init 0 */

  FSMC_NORSRAM_TimingTypeDef Timing = {0};

  /* USER CODE BEGIN FSMC_Init 1 */

  /* USER CODE END FSMC_Init 1 */

  /** Perform the SRAM1 memory initialization sequence
  */
  hsram1.Instance = FSMC_NORSRAM_DEVICE;
  hsram1.Extended = FSMC_NORSRAM_EXTENDED_DEVICE;
  /* hsram1.Init */
  hsram1.Init.NSBank = FSMC_NORSRAM_BANK1;
  hsram1.Init.DataAddressMux = FSMC_DATA_ADDRESS_MUX_DISABLE;
  hsram1.Init.MemoryType = FSMC_MEMORY_TYPE_SRAM;
  hsram1.Init.MemoryDataWidth = FSMC_NORSRAM_MEM_BUS_WIDTH_16;
  hsram1.Init.BurstAccessMode = FSMC_BURST_ACCESS_MODE_DISABLE;
  hsram1.Init.WaitSignalPolarity = FSMC_WAIT_SIGNAL_POLARITY_LOW;
  hsram1.Init.WrapMode = FSMC_WRAP_MODE_DISABLE;
  hsram1.Init.WaitSignalActive = FSMC_WAIT_TIMING_BEFORE_WS;
  hsram1.Init.WriteOperation = FSMC_WRITE_OPERATION_ENABLE;
  hsram1.Init.WaitSignal = FSMC_WAIT_SIGNAL_DISABLE;
  hsram1.Init.ExtendedMode = FSMC_EXTENDED_MODE_DISABLE;
  hsram1.Init.AsynchronousWait = FSMC_ASYNCHRONOUS_WAIT_DISABLE;
  hsram1.Init.WriteBurst = FSMC_WRITE_BURST_DISABLE;
  hsram1.Init.PageSize = FSMC_PAGE_SIZE_NONE;
  /* Timing */
  Timing.AddressSetupTime = 0;
  Timing.AddressHoldTime = 15;
  Timing.DataSetupTime = 1;
  Timing.BusTurnAroundDuration = 0;
  Timing.CLKDivision = 16;
  Timing.DataLatency = 17;
  Timing.AccessMode = FSMC_ACCESS_MODE_A;
  /* ExtTiming */

  if (HAL_SRAM_Init(&hsram1, &Timing, NULL) != HAL_OK)
  {
    Error_Handler( );
  }

  /* USER CODE BEGIN FSMC_Init 2 */

  /* USER CODE END FSMC_Init 2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
