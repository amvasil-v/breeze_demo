#ifndef ESP_MODEM_H
#define ESP_MODEM_H

#include "esp_uart.h"

struct _esp_modem_t {
    esp_bridge_t *br;
    char *tx_buf;
    char *rx_buf;
    char *rx_wptr;
    char *rx_rptr;
    size_t ipd_len;
};
typedef struct _esp_modem_t esp_modem_t;

int esp_modem_tcp_connect(esp_modem_t *esp, const char *host, const char *port, uint32_t timeout);
void esp_modem_init(esp_modem_t *esp, esp_bridge_t *bridge);
void esp_modem_release(esp_modem_t *esp);
int esp_modem_tcp_send(esp_modem_t *esp, const char *buf, size_t len);
int esp_modem_tcp_receive(esp_modem_t *esp, char *buf, size_t len, uint32_t timeout);
int esp_modem_tcp_close(esp_modem_t *esp);

#endif
