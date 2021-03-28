/*
 * debug_uart.h
 *
 *  Created on: Mar 28, 2021
 *      Author: casca
 */

#include "main.h"

#include <string.h>

extern UART_HandleTypeDef huart6;
extern DMA_HandleTypeDef hdma_usart6_tx;

static int uart_send_buf(const char *buf, size_t len)
{
	if (HAL_UART_GetState(&huart6) != HAL_UART_STATE_READY)
		return -1;
	if (HAL_DMA_GetState(&hdma_usart6_tx) == HAL_DMA_STATE_BUSY) {
		HAL_Delay(10);
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
