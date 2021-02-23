#include "esp_modem.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "esp_utils.h"

#define ESP_MODEM_RX_BUF_SIZE 256
#define ESP_MODEM_RX_DATA_BUF_SIZE 4096

#define MAX(X,Y) ((X)>(Y)?(X):(Y))
#define MIN(X,Y) ((X)<(Y)?(X):(Y))

static const char *esp_modem_ipd_str = "\r\n+IPD,";
static const char *esp_ok_str = "\r\nOK\r\n";
static const char *esp_error_str = "\r\nERROR\r\n";

static uint8_t esp_receive_buf_complete(const char *buf, size_t len)
{
    
    return esp_find_substr(buf, len, esp_ok_str, strlen(esp_ok_str)) ||
           esp_find_substr(buf, len, esp_error_str, strlen(esp_error_str));
}

static uint8_t esp_receive_buf_ready_tx(const char *buf, size_t len)
{
    return esp_find_substr(buf, len, esp_ok_str, strlen(esp_ok_str)) != NULL;
}

static void esp_reset_rx(esp_modem_t *esp)
{
    esp->rx_rptr = esp->rx_wptr = esp->rx_buf;
    esp->ipd_len = 0;
}

void esp_modem_init(esp_modem_t *esp, esp_bridge_t *bridge)
{
    esp->br = bridge;
    esp->rx_buf = (char *)malloc(2048);
    esp->tx_buf = (char *)malloc(ESP_MODEM_RX_BUF_SIZE);
}

void esp_modem_release(esp_modem_t *esp)
{
    free(esp->rx_buf);
    free(esp->tx_buf);
}

int esp_modem_tcp_connect(esp_modem_t *esp, const char *host, const char *port, uint32_t timeout)
{
    uint32_t tim;
    uint32_t read_timeout = 10;
    int res;
    size_t rx_len = 0;
    static const char *connect_str = "CONNECT\r\n";
    static const char *acon_str = "ALREADY CONNECTED\r\n";
    if (!esp || !esp_bridge_connected(esp->br))
        return -1;
    esp_reset_rx(esp);

    sprintf(esp->tx_buf, "AT+CIPSTART=\"TCP\",\"%s\",%s\r\n", host, port);
    res = esp_bridge_write(esp->br, esp->tx_buf, strlen(esp->tx_buf));
    if (res < 0)
        return -1;
    char *ptr = esp->rx_buf;
    for (tim = 0; tim < timeout; tim += read_timeout) {
        res = esp_bridge_read_timeout(esp->br, ptr, 128, read_timeout);
        if (res <= 0)
            continue;
        ptr += res;
        rx_len += res;
        if (esp_receive_buf_complete(esp->rx_buf, rx_len)) {
            if (esp_find_substr(esp->rx_buf, rx_len, connect_str, strlen(connect_str)) ||
                esp_find_substr(esp->rx_buf, rx_len, acon_str, strlen(acon_str))) {
                    esp_debug(ESP_DBG_WARN, "[ESP] TCP: connected to %s:%s\n", host, port);
                    return 0;
                }
            esp_debug(ESP_DBG_ERROR, "Received invalid response: %s\n", esp->rx_buf);
            return -1;
        }
        if (ptr > esp->rx_buf + ESP_MODEM_RX_BUF_SIZE)
            return -1;
    }
    esp_debug(ESP_DBG_ERROR, "Connection timeout\n");
    return -1;
}

static int esp_confirm_tcp_send(esp_modem_t *esp)
{
    int res;
    int i;
    char *confirm;
    static const char *confirm_str = "\r\nSEND OK\r\n";
    size_t rx_len = 0;

    esp_reset_rx(esp);

    for (i = 0; i < 10; i++) {
        res = esp_bridge_read_timeout(esp->br, esp->rx_wptr, 43, 100);
        if (res <= 0)
            continue;
        esp->rx_wptr += res;
        rx_len += res;
        confirm = esp_find_substr(esp->rx_buf, rx_len, confirm_str, strlen(confirm_str));
        if (confirm) {
            esp_debug(ESP_DBG_INFO, "[ESP] TX confirmed\n");
            return 0;
        }
    }
    esp_debug(ESP_DBG_ERROR, "TX confirm timeout\n");
    return -1;
}

int esp_modem_tcp_send(esp_modem_t *esp, const char *buf, size_t len)
{
    int res;
    int i;
    if (!esp || !esp_bridge_connected(esp->br))
        return -1;
    sprintf(esp->tx_buf, "AT+CIPSEND=%d\r\n", len);
    esp_reset_rx(esp);
    res = esp_bridge_write(esp->br, esp->tx_buf, strlen(esp->tx_buf));
    if (res < 0)
        return -1;

    char *ptr = esp->rx_buf;
    for (i = 0; i < 10; i++) {
        res = esp_bridge_read_timeout(esp->br, ptr, 128, 10);
        if (res <= 0)
            continue;
        ptr += res;
        if (esp_receive_buf_ready_tx(esp->rx_buf, ptr - esp->rx_buf)) {
            res = esp_bridge_write(esp->br, buf, len);
            if (res < 0)
                return -1;
            if (!esp_confirm_tcp_send(esp))
                return len;
            else
                return -1;
        }
    }
    esp_debug(ESP_DBG_ERROR, "TX timeout\n");
    return -1;
}

#define ESP_MODEM_IPD_MAX_LEN       4
#define ESP_MODEM_IPD_MAX_VALUE     3000
#define ESP_MODEM_READ_CHUNK_SIZE   128

static const char *esp_modem_get_ipd(const char *str, uint32_t *out_length)
{
    int i;
    char buf[ESP_MODEM_IPD_MAX_LEN + 1] = {0};
    int out;

    if (*str == '\r')
        str++;
    if (*str == '\n')
        str++;

    for (i = 1; i <= 4; i++) {
        if (str[i] == ':') {
            strncpy(buf, str, i);
            out = atoi(buf);
            if (out <= 0 || out >= ESP_MODEM_IPD_MAX_VALUE)
                return NULL;
            *out_length = out;
            return &str[i+1];
        }
    }

    return NULL;
}

static void copy_bytes(char *dst, char *from, char *to)
{
    while (from != to) {
        *dst++ = *from++;
    }
}

static int esp_modem_receive_data(esp_modem_t *esp, char *out_buf, size_t req_len, uint32_t timeout)
{
    size_t received = MIN(esp->rx_wptr - esp->rx_rptr, esp->ipd_len);
    size_t to_read = MIN(received, req_len);
    size_t read_bytes = 0;
    char *out_ptr = out_buf;

    memcpy(out_ptr, esp->rx_rptr, to_read);    
    read_bytes = to_read;
    out_ptr += read_bytes;
    esp->rx_rptr += read_bytes;

    if (esp->ipd_len < read_bytes) {
        esp_debug(ESP_DBG_ERROR, "Read error: saved ipd_len %d < read_bytes %d\n", esp->ipd_len, read_bytes);
        esp_reset_rx(esp);
        return -1;
    }
    esp->ipd_len -= read_bytes;
    if (esp->ipd_len == 0) {
        esp_reset_rx(esp);
        return read_bytes;
    }
    if (read_bytes == req_len)
        return read_bytes;

    size_t rem_len = MIN(req_len - read_bytes, esp->ipd_len);
    int res = esp_bridge_read_timeout(esp->br, esp->rx_wptr, rem_len, timeout);
    if (res <= 0)
    {
        esp_debug(ESP_DBG_ERROR, "TCP read data timeout %d\n", res);
        return -1;
    }
    read_bytes += res;
    memcpy(out_ptr, esp->rx_rptr, res);
    esp->rx_wptr += res;
    esp->rx_rptr = esp->rx_wptr;
    if (esp->ipd_len < res) {
        esp_debug(ESP_DBG_ERROR, "Read error: ipd_len < read_bytes\n");
        esp_reset_rx(esp);
        return -1;
    }
    esp->ipd_len -= res;
    if (esp->ipd_len == 0) {
        esp_reset_rx(esp);
    }   

    return read_bytes;    
}

static int esp_modem_receive_ipd(esp_modem_t *esp, uint32_t timeout)
{
    int res;
    char *ipd_ptr;
    char *data_ptr;
    uint32_t tim = 0;
    uint32_t ipd_len = 0;
    const uint32_t read_timeout = timeout > 100 ? 100 : timeout;

    while (tim < timeout) {
        res = esp_bridge_read_timeout(esp->br, esp->rx_wptr, strlen(esp_modem_ipd_str) + 6, read_timeout);
        if (res <= 0) {
            tim += read_timeout;
            continue;
        }
        esp->rx_wptr += res;
        ipd_ptr = esp_find_substr(esp->rx_buf, esp->rx_wptr - esp->rx_buf, esp_modem_ipd_str, strlen(esp_modem_ipd_str));
        if (!ipd_ptr) {
            continue;
        }
        data_ptr = (char *)esp_modem_get_ipd(ipd_ptr + strlen(esp_modem_ipd_str), &ipd_len);
        if (!data_ptr) {
            esp_debug(ESP_DBG_ERROR, "Failed to parse IPD\n");
            return -1;
        }
        esp_debug(ESP_DBG_INFO, "[ESP] Received IPD %d\n", ipd_len);
        esp->rx_rptr = data_ptr;       
        esp->ipd_len = ipd_len;
        return 0;
    };

    esp_debug(ESP_DBG_ERROR, "TCP IPD read timeout\n");
    return -1;
}

int esp_modem_tcp_receive(esp_modem_t *esp, char *buf, size_t len, uint32_t timeout)
{
    int res;

    if (len == 0)
        return 0;
    if (!esp || !esp_bridge_connected(esp->br))
        return -1;

    if (!esp->ipd_len)
    {
        res = esp_modem_receive_ipd(esp, timeout);
        if (res)
            return -1;
    }
    if (esp->ipd_len)
    {
        return esp_modem_receive_data(esp, buf, len, timeout);
    }

    esp_debug(ESP_DBG_ERROR, "TCP read timeout\n");
    return -1;
}

int esp_modem_tcp_close(esp_modem_t *esp)
{
    static const char *cipclose_str = "AT+CIPCLOSE\r\n";
    if (!esp || !esp_bridge_connected(esp->br))
        return -1;
    int res = esp_bridge_write(esp->br, cipclose_str, strlen(cipclose_str));
    if (res < 0)
        return -1;
    esp_debug(ESP_DBG_WARN, "[ESP] Sent CIPCLOSE\n");
    return 0;
}

