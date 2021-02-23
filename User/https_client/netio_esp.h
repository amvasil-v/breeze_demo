#ifndef NETIO_ESP_H
#define NETIO_ESP_H

#include "netio.h"
#include "esp_uart.h"
#include "esp_modem.h"

struct _netio_esp_t
{
    netio_t io;
    esp_bridge_t br;
    esp_modem_t esp;
};

typedef struct _netio_esp_t netio_esp_t;

netio_t *netio_esp_create(void);
int netio_esp_init(netio_t *io);
void netio_esp_free(netio_t *io);

#endif
