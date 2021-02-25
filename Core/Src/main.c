/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * <h2><center>&copy; Copyright (c) 2021 STMicroelectronics.
 * All rights reserved.</center></h2>
 *
 * This software component is licensed by ST under BSD 3-Clause license,
 * the "License"; You may not use this file except in compliance with the
 * License. You may obtain a copy of the License at:
 *                        opensource.org/licenses/BSD-3-Clause
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>
#include <stdio.h>

#include "display.h"
#include "qspi_driver.h"
#include "esp_uart.h"
#include "wifi_cred.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
#define LED_GPIO_ON		GPIO_PIN_RESET
#define LED_GPIO_OFF	(!(LED_GPIO_ON))

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define PNG_IMAGE_ADDRESS		0x08020000
#define PNG_IMAGE_SIZE			53793
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
QSPI_HandleTypeDef hqspi;

SPI_HandleTypeDef hspi2;

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart6;
DMA_HandleTypeDef hdma_usart1_rx;
DMA_HandleTypeDef hdma_usart1_tx;
DMA_HandleTypeDef hdma_usart6_tx;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_USART6_UART_Init(void);
static void MX_SPI2_Init(void);
static void MX_QUADSPI_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */
static void qspi_test(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static int uart_send_buf(const char *buf, size_t len)
{
	if (HAL_UART_GetState(&huart6) != HAL_UART_STATE_READY)
		return -1;
	if (HAL_DMA_GetState(&hdma_usart6_tx) == HAL_DMA_STATE_BUSY) {
		HAL_Delay(100);
		if (HAL_DMA_GetState(&hdma_usart6_tx) == HAL_DMA_STATE_BUSY) {
			return -1;
		}
	}
	int res =  HAL_UART_Transmit_DMA(&huart6, (uint8_t *)buf, (uint16_t)len);
	if (res != HAL_OK)
		return -1;
	for (int i=0; i<10; i++) {
		if (HAL_DMA_GetState(&hdma_usart6_tx) == HAL_DMA_STATE_READY)
			return 0;
		HAL_Delay(10);
	}
	return -1;
}

#define DBG_UART_BUF_LEN		256

int _write (int file, char *data, int len)
{
	static char uart_buf[DBG_UART_BUF_LEN];
	static size_t uart_buf_size = 0;

	size_t to_copy = len;
	if (to_copy + uart_buf_size > DBG_UART_BUF_LEN)
		to_copy = DBG_UART_BUF_LEN - uart_buf_size;
	if (to_copy) {
		memcpy(&uart_buf[uart_buf_size], data, to_copy);
		uart_buf_size += to_copy;
	}

	if (uart_buf_size == DBG_UART_BUF_LEN || uart_buf[uart_buf_size - 1] == '\n') {
		int res = uart_send_buf(uart_buf, uart_buf_size);
		uart_buf_size = 0;
		return res == 0 ? len : 0;
	}

	return len;


}
/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void)
{
	/* USER CODE BEGIN 1 */

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
	MX_DMA_Init();
	MX_USART6_UART_Init();
	MX_SPI2_Init();
	MX_QUADSPI_Init();
	MX_USART1_UART_Init();
	/* USER CODE BEGIN 2 */

	/* USER CODE END 2 */

	/* Infinite loop */
	/* USER CODE BEGIN WHILE */
	HAL_Delay(400);
	printf("App started\r\n");
	HAL_Delay(100);
	led_set(0, 0);
	led_set(1, 0);

	if (QSPI_init_device()) {
		printf("Failed to init QSPI flash\r\n");
		Error_Handler();
	}

	esp_bridge_t *esp = esp_bridge_create();
	int esp_ret;
	const char *ssid = ESP_WIFI_SSID;
	uint8_t do_connect = 0;
	printf("ESP: Waiting for boot\r\n");
	HAL_Delay(4000);

	if (esp_command_ate0(esp)) {
		printf("ESP: Failed to set ATE0\r\n");
		Error_Handler();
	} else {
		printf("ESP: set ATE0\r\n");
	}

	esp_ret = esp_command_cwjap_query(esp, ssid);
	if (esp_ret < 0) {
		Error_Handler();
	}
	if (esp_ret == 0) {
		printf("ESP: Connect to AP:\r\n");
		do_connect = 1;
	}
	if (esp_ret != 1) {
		printf("ESP: Invalid ESP state\r\n");
		Error_Handler();
	}

	if (do_connect) {
		esp_command_cwjap_connect(esp,ssid,ESP_WIFI_PASSWD);
		esp_ret = esp_command_cwjap_query(esp, ssid);
		if (esp_ret != 1) {
			printf("ESP: Failed to connect to AP\r\n");
			Error_Handler();
		}
	}

	printf("ESP: Connected to AP %s\r\n", ssid);

	//#define PROGRAM_IMAGE_QSPI
#ifdef PROGRAM_IMAGE_QSPI
	const size_t qspi_addr = 0;

	if (QSPI_area_erase(qspi_addr, PNG_IMAGE_SIZE)) {
		printf("Failed to erase QSPI flash\r\n");
		Error_Handler();
	}
	HAL_Delay(100);
	size_t bytes_written = 0;
	const size_t write_chunk = QSPI_PAGE_SIZE;
	static uint8_t write_buf[QSPI_PAGE_SIZE];
	while (bytes_written < PNG_IMAGE_SIZE) {
		size_t to_write = PNG_IMAGE_SIZE - bytes_written;
		if (to_write > write_chunk) {
			to_write = write_chunk;
		}
		memcpy(write_buf, (uint8_t *)PNG_IMAGE_ADDRESS + bytes_written, to_write);
		if (QSPI_write_page(write_buf, to_write, qspi_addr + bytes_written)) {
			Error_Handler();
		}
		bytes_written += to_write;
	}
	printf("Written PNG image to QSPI flash\r\n");
#endif

#ifdef DISPLAY_IMAGE
	led_set(1, 1);
	load_png_image_init();
	size_t bytes_fed = 0;
	const size_t feed_chunk = QSPI_PAGE_SIZE;
	static uint8_t qspi_read_buf[QSPI_PAGE_SIZE];
	while (bytes_fed < PNG_IMAGE_SIZE) {
		size_t to_feed = PNG_IMAGE_SIZE - bytes_fed;
		if (to_feed > feed_chunk) {
			to_feed = feed_chunk;
		}
		if (QSPI_read_page(qspi_read_buf, to_feed, qspi_addr + bytes_fed)) {
			Error_Handler();
		}
		if (load_png_image_feed(qspi_read_buf, to_feed)) {
			printf("Feed failed at %u\r\n", bytes_fed);
			Error_Handler();
		}
		bytes_fed += to_feed;
	}
	load_png_image_release();

	display_init();
	display_png_image();
	HAL_Delay(100);
	display_deinit();
#endif

	while (1)
	{
		/* USER CODE END WHILE */

		/* USER CODE BEGIN 3 */
		HAL_GPIO_TogglePin(LED1_GPIO_Port, LED1_Pin);
		HAL_Delay(400);

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
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
	RCC_OscInitStruct.HSEState = RCC_HSE_ON;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
	RCC_OscInitStruct.PLL.PLLM = 4;
	RCC_OscInitStruct.PLL.PLLN = 180;
	RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
	RCC_OscInitStruct.PLL.PLLQ = 2;
	RCC_OscInitStruct.PLL.PLLR = 2;
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
	{
		Error_Handler();
	}
	/** Activate the Over-Drive mode
	 */
	if (HAL_PWREx_EnableOverDrive() != HAL_OK)
	{
		Error_Handler();
	}
	/** Initializes the CPU, AHB and APB buses clocks
	 */
	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
			|RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
	{
		Error_Handler();
	}
}

/**
 * @brief QUADSPI Initialization Function
 * @param None
 * @retval None
 */
static void MX_QUADSPI_Init(void)
{

	/* USER CODE BEGIN QUADSPI_Init 0 */

	/* USER CODE END QUADSPI_Init 0 */

	/* USER CODE BEGIN QUADSPI_Init 1 */

	/* USER CODE END QUADSPI_Init 1 */
	/* QUADSPI parameter configuration*/
	hqspi.Instance = QUADSPI;
	hqspi.Init.ClockPrescaler = 127;
	hqspi.Init.FifoThreshold = 1;
	hqspi.Init.SampleShifting = QSPI_SAMPLE_SHIFTING_NONE;
	hqspi.Init.FlashSize = 21;
	hqspi.Init.ChipSelectHighTime = QSPI_CS_HIGH_TIME_1_CYCLE;
	hqspi.Init.ClockMode = QSPI_CLOCK_MODE_0;
	hqspi.Init.FlashID = QSPI_FLASH_ID_1;
	hqspi.Init.DualFlash = QSPI_DUALFLASH_DISABLE;
	if (HAL_QSPI_Init(&hqspi) != HAL_OK)
	{
		Error_Handler();
	}
	/* USER CODE BEGIN QUADSPI_Init 2 */

	/* USER CODE END QUADSPI_Init 2 */

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
	hspi2.Init.Direction = SPI_DIRECTION_1LINE;
	hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
	hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;
	hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;
	hspi2.Init.NSS = SPI_NSS_SOFT;
	hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_32;
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
 * @brief USART1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_USART1_UART_Init(void)
{

	/* USER CODE BEGIN USART1_Init 0 */

	/* USER CODE END USART1_Init 0 */

	/* USER CODE BEGIN USART1_Init 1 */

	/* USER CODE END USART1_Init 1 */
	huart1.Instance = USART1;
	huart1.Init.BaudRate = 115200;
	huart1.Init.WordLength = UART_WORDLENGTH_8B;
	huart1.Init.StopBits = UART_STOPBITS_1;
	huart1.Init.Parity = UART_PARITY_NONE;
	huart1.Init.Mode = UART_MODE_TX_RX;
	huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	huart1.Init.OverSampling = UART_OVERSAMPLING_16;
	if (HAL_UART_Init(&huart1) != HAL_OK)
	{
		Error_Handler();
	}
	/* USER CODE BEGIN USART1_Init 2 */

	/* USER CODE END USART1_Init 2 */

}

/**
 * @brief USART6 Initialization Function
 * @param None
 * @retval None
 */
static void MX_USART6_UART_Init(void)
{

	/* USER CODE BEGIN USART6_Init 0 */

	/* USER CODE END USART6_Init 0 */

	/* USER CODE BEGIN USART6_Init 1 */

	/* USER CODE END USART6_Init 1 */
	huart6.Instance = USART6;
	huart6.Init.BaudRate = 115200;
	huart6.Init.WordLength = UART_WORDLENGTH_8B;
	huart6.Init.StopBits = UART_STOPBITS_1;
	huart6.Init.Parity = UART_PARITY_NONE;
	huart6.Init.Mode = UART_MODE_TX_RX;
	huart6.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	huart6.Init.OverSampling = UART_OVERSAMPLING_16;
	if (HAL_UART_Init(&huart6) != HAL_OK)
	{
		Error_Handler();
	}
	/* USER CODE BEGIN USART6_Init 2 */

	/* USER CODE END USART6_Init 2 */

}

/**
 * Enable DMA controller clock
 */
static void MX_DMA_Init(void)
{

	/* DMA controller clock enable */
	__HAL_RCC_DMA2_CLK_ENABLE();

	/* DMA interrupt init */
	/* DMA2_Stream2_IRQn interrupt configuration */
	HAL_NVIC_SetPriority(DMA2_Stream2_IRQn, 0, 0);
	HAL_NVIC_EnableIRQ(DMA2_Stream2_IRQn);
	/* DMA2_Stream6_IRQn interrupt configuration */
	HAL_NVIC_SetPriority(DMA2_Stream6_IRQn, 0, 0);
	HAL_NVIC_EnableIRQ(DMA2_Stream6_IRQn);
	/* DMA2_Stream7_IRQn interrupt configuration */
	HAL_NVIC_SetPriority(DMA2_Stream7_IRQn, 0, 0);
	HAL_NVIC_EnableIRQ(DMA2_Stream7_IRQn);

}

/**
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};

	/* GPIO Ports Clock Enable */
	__HAL_RCC_GPIOC_CLK_ENABLE();
	__HAL_RCC_GPIOH_CLK_ENABLE();
	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_GPIOB_CLK_ENABLE();

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(WRST_GPIO_Port, WRST_Pin, GPIO_PIN_RESET);

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(LED0_GPIO_Port, LED0_Pin, GPIO_PIN_SET);

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_SET);

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(GPIOB, WCS_Pin|WCMD_Pin, GPIO_PIN_RESET);

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(GPIOA, ESPRST_Pin|ESPBOOT_Pin, GPIO_PIN_SET);

	/*Configure GPIO pin : WRST_Pin */
	GPIO_InitStruct.Pin = WRST_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(WRST_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pin : LED0_Pin */
	GPIO_InitStruct.Pin = LED0_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(LED0_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pins : LED1_Pin WCS_Pin WCMD_Pin */
	GPIO_InitStruct.Pin = LED1_Pin|WCS_Pin|WCMD_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

	/*Configure GPIO pin : WBUSY_Pin */
	GPIO_InitStruct.Pin = WBUSY_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(WBUSY_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pins : ESPRST_Pin ESPBOOT_Pin */
	GPIO_InitStruct.Pin = ESPRST_Pin|ESPBOOT_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
	GPIO_InitStruct.Pull = GPIO_PULLUP;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

}

/* USER CODE BEGIN 4 */
void led_blink(uint8_t led)
{
	if (led == 0) {
		HAL_GPIO_TogglePin(LED0_GPIO_Port, LED0_Pin);
	} else if (led == 1) {
		HAL_GPIO_TogglePin(LED1_GPIO_Port, LED1_Pin);
	}
}

void led_set(uint8_t led, uint8_t state)
{
	GPIO_PinState led_state = state ? LED_GPIO_ON : LED_GPIO_OFF;
	if (led == 0) {
		HAL_GPIO_WritePin(LED0_GPIO_Port, LED0_Pin, led_state);
	} else if (led == 1) {
		HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, led_state);
	}
}

__attribute__ ((unused))
static void qspi_test(void)
{
	const size_t pattern_size = 150000;
	const size_t flash_addr = 0x0;

	if (QSPI_init_device()) {
		printf("Failed to init QSPI flash\r\n");
		Error_Handler();
	}
	if (QSPI_area_erase(flash_addr, pattern_size)) {
		printf("Failed to erase QSPI flash\r\n");
		Error_Handler();
	}
	HAL_Delay(100);
	if (QSPI_write_pattern(flash_addr, pattern_size)) {
		printf("Failed to write pattern\r\n");
		Error_Handler();
	}
	HAL_Delay(100);
	led_set(1, 1);
	if (QSPI_verify_pattern(flash_addr, pattern_size)) {
		printf("Failed to verify pattern\r\n");
		Error_Handler();
	}
	led_set(0, 0);
}
/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void)
{
	/* USER CODE BEGIN Error_Handler_Debug */
	/* User can add his own implementation to report the HAL error return state */
	HAL_GPIO_WritePin(LED0_GPIO_Port, LED0_Pin, LED_GPIO_ON);
	HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, LED_GPIO_ON);
	while (1) {
		HAL_GPIO_TogglePin(LED0_GPIO_Port, LED0_Pin);
		HAL_GPIO_TogglePin(LED1_GPIO_Port, LED1_Pin);
		for (int i = 0; i < 1000000; i++) {
			__NOP();
		}
	}
	/* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
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
     tex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
	/* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
