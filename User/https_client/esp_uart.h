/*
 * esp_uart.h
 *
 *  Created on: 23 февр. 2021 г.
 *      Author: alexm
 */

#ifndef HTTPS_CLIENT_ESP_UART_H_
#define HTTPS_CLIENT_ESP_UART_H_

#include <stdint.h>
#include <stddef.h>

#define ESP_UART_DEFAULT_TIMEOUT		1000

struct _esp_bridge_t {
	int id;
};
typedef struct _esp_bridge_t esp_bridge_t;

esp_bridge_t *esp_bridge_create(void);
void esp_bridge_init(esp_bridge_t * br);
int esp_bridge_check(esp_bridge_t * br);
void esp_bridge_disconnect(esp_bridge_t * br);
void esp_bridge_free(esp_bridge_t * br);
int esp_bridge_write(esp_bridge_t * br, const char *buf, size_t len);
int esp_bridge_read_timeout(esp_bridge_t * br, char *buf, size_t len, uint32_t timeout);
uint8_t esp_bridge_connected(esp_bridge_t * br);

int esp_command_ate0(esp_bridge_t *esp);

#endif /* HTTPS_CLIENT_ESP_UART_H_ */