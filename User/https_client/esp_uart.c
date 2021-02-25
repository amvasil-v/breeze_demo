/*
 * esp_uart.c
 *
 *  Created on: 23 февр. 2021 г.
 *      Author: alexm
 */

#include "esp_uart.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "main.h"

extern UART_HandleTypeDef huart1;
extern DMA_HandleTypeDef hdma_usart1_rx;
extern DMA_HandleTypeDef hdma_usart1_tx;

#define ESP_UART_RX_BUF_SIZE		1024

static uint32_t esp_rx_buf_place[ESP_UART_RX_BUF_SIZE / sizeof(uint32_t)];
static uint8_t * const esp_rx_buffer = (uint8_t * const)esp_rx_buf_place;
static size_t esp_rx_read_it = 0;
static uint8_t esp_rx_wrapped = 0;

esp_bridge_t *esp_bridge_create(void)
{
	esp_bridge_t *br = (esp_bridge_t *)malloc(sizeof(esp_bridge_t));
	esp_bridge_init(br);
	return br;
}

void esp_bridge_init(esp_bridge_t * br)
{
	memset(br, 0, sizeof(esp_bridge_t));
	br->id = 1;

	HAL_GPIO_WritePin(ESPRST_GPIO_Port, ESPRST_Pin, GPIO_PIN_RESET);
	HAL_Delay(100);
	HAL_GPIO_WritePin(ESPRST_GPIO_Port, ESPRST_Pin, GPIO_PIN_SET);
	HAL_Delay(500);

	esp_rx_read_it = 0;
	esp_rx_wrapped = 0;
	if (HAL_UART_Receive_DMA(&huart1, esp_rx_buffer, ESP_UART_RX_BUF_SIZE) != HAL_OK) {
		printf("Failed to start ESP DMA\r\n");
		Error_Handler();
	}
}

int esp_bridge_check(esp_bridge_t * br)
{
	return 0;
}

void esp_bridge_disconnect(esp_bridge_t * br)
{
}

void esp_bridge_free(esp_bridge_t *br)
{
	esp_bridge_disconnect(br);
	free(br);
}

uint8_t esp_bridge_connected(esp_bridge_t * br)
{
	return !!br;
}

int esp_bridge_write(esp_bridge_t *br, const char *buf, size_t len)
{
	if (HAL_UART_Transmit(&huart1, (uint8_t *)buf, len, ESP_UART_DEFAULT_TIMEOUT) != HAL_OK) {
		return -1;
	}
	return 0;
}

static int esp_uart_try_receive(size_t len, char *buf)
{
	int copied = 0;
	if (esp_rx_wrapped) {
		size_t to_copy = ESP_UART_RX_BUF_SIZE - esp_rx_read_it;
		if (to_copy >= len) {
			to_copy = len;
			esp_rx_wrapped = 0;
			memcpy(buf, &esp_rx_buffer[esp_rx_read_it], to_copy);
			esp_rx_read_it = 0;
		} else if (to_copy > 0) {
			memcpy(buf, &esp_rx_buffer[esp_rx_read_it], to_copy);
			esp_rx_read_it += to_copy;
		}
		copied = to_copy;
	} else {
		size_t dma_received = ESP_UART_RX_BUF_SIZE - hdma_usart1_rx.Instance->NDTR;
		size_t to_copy = dma_received - esp_rx_read_it;
		if (to_copy > len) {
			to_copy = len;
		}
		if (to_copy > 0) {
			memcpy(buf, &esp_rx_buffer[esp_rx_read_it], to_copy);
			esp_rx_read_it += to_copy;
			copied = to_copy;
		}
	}
	return copied;
}

int esp_bridge_read_timeout(esp_bridge_t * br, char *buf, size_t len, uint32_t timeout)
{
	uint32_t start_tick = HAL_GetTick();
	int copied = 0;
	do {
		copied += esp_uart_try_receive(len - (size_t)copied, &buf[copied]);
		HAL_Delay(1);
	} while (HAL_GetTick() - start_tick <= timeout);
	return copied;
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
	if (huart->Instance == huart1.Instance) {
		esp_rx_wrapped = 1;
	}
}

#define ESP_COMMAND_RESP_BUF_SIZE		256

static char command_resp[ESP_COMMAND_RESP_BUF_SIZE];

int esp_flush_input(esp_bridge_t *esp)
{
	while (1) {
		int ret = esp_bridge_read_timeout(esp, command_resp, ESP_COMMAND_RESP_BUF_SIZE, 0);
		if (ret <= 0)
			break;
	}
	return 0;
}

static int esp_at_cmd(esp_bridge_t *esp, const char *cmd, uint32_t delay)
{
	esp_bridge_write(esp, cmd, strlen(cmd));
	HAL_Delay(delay);
	int ret = esp_bridge_read_timeout(esp, command_resp, ESP_COMMAND_RESP_BUF_SIZE, 200);
	if (ret > 0) {
		command_resp[ret] = 0;
		printf("Rx %d: %s", ret, command_resp);
		return 0;
	}
	printf("No response for AT cmd %s\n", cmd);
	return -1;

}

int esp_command_ate0(esp_bridge_t *esp)
{
	static const char *cmd = "ATE0\r\n";
	if (esp_at_cmd(esp, cmd, 100)) {
		return -1;
	}
	if (!strstr(command_resp, "OK\r\n"))
		return -1;
	return 0;
}

int esp_command_cwjap_query(esp_bridge_t *esp, const char *ssid)
{
	static const char *cmd = "AT+CWJAP?\r\n";
	if (esp_at_cmd(esp, cmd, 1000)) {
		return -1;
	}
	if (strstr(command_resp, "No AP\r\n"))
		return 0;
	if (strstr(command_resp, ssid))
		return 1;
	return 2;
}

int esp_command_cwjap_connect(esp_bridge_t *esp, const char *ssid, const char *passwd)
{
	sprintf(command_resp, "AT+CWJAP=\"%s\",\"%s\"\r\n", ssid, passwd);
	if (esp_at_cmd(esp, command_resp, 3000)) {
		return -1;
	}
	return 0;
}

