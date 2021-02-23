#include "netio_esp.h"

#include <stdlib.h>
#include <stdio.h>

#include "esp_utils.h"

static int netio_esp_send(void *ctx, const unsigned char *buf, size_t len)
{
    netio_esp_t *io = (netio_esp_t *)ctx;
    esp_debug(ESP_DBG_INFO, "[ESP] Send %lu bytes\n", len);
    return esp_modem_tcp_send(&io->esp, buf, len);
}

static int netio_esp_recv_timeout(void *ctx, unsigned char *buf, size_t len, uint32_t timeout)
{
    netio_esp_t *io = (netio_esp_t *)ctx;
    int res = esp_modem_tcp_receive(&io->esp, buf, len, timeout);
    esp_debug(ESP_DBG_INFO, "[ESP] Received %d\n", res);
    return res;
}

static int netio_esp_connect(void *ctx, const char *host, const char *port)
{
    netio_esp_t *io = (netio_esp_t *)ctx;
    return esp_modem_tcp_connect(&io->esp, host, port, 2000);
}

static int netio_esp_disconnect(void *ctx)
{
    netio_esp_t *io = (netio_esp_t *)ctx;
    return esp_modem_tcp_close(&io->esp);
}

static uint8_t netio_esp_connected(void *ctx)
{
    netio_t *io = (netio_t *)ctx;
    esp_bridge_t *br = (esp_bridge_t *)io->ctx;
    uint8_t connected = esp_bridge_connected(br);
    return connected;
} 

netio_t *netio_esp_create(void)
{
    netio_esp_t *ns_io = (netio_esp_t *)malloc(sizeof(netio_esp_t));
    netio_t *io = &ns_io->io;

    _Static_assert(offsetof(netio_esp_t, io) == 0, "Offset of netio_esp_t->io must be 0");
    io->send = netio_esp_send;
    io->recv_timeout = netio_esp_recv_timeout;
    io->connect = netio_esp_connect;
    io->opened = netio_esp_connected;
    io->connected = netio_esp_connected;
    io->disconnect = netio_esp_disconnect;
    io->ctx = &ns_io->br;    
    esp_bridge_init((esp_bridge_t *)io->ctx);
    esp_modem_init(&ns_io->esp, &ns_io->br);
    return io;
}


int netio_esp_init(netio_t *io)
{
    esp_bridge_t *br = (esp_bridge_t *)io->ctx;
    return esp_bridge_check(br);
}


void netio_esp_free(netio_t *io)
{
    esp_bridge_t *br = (esp_bridge_t *)io->ctx;
    return esp_bridge_disconnect(br);
    free(io);
}
