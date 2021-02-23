#ifndef NETIO_H
#define NETIO_H

#include <stddef.h>
#include <stdint.h>

struct _netio_t {
    void *ctx;
    int (*send)(void *ctx, const unsigned char *buf, size_t len);
    int (*recv_timeout)(void *ctx, unsigned char *buf, size_t len, uint32_t timeout);
    int (*connect)(void *ctx, const char *host, const char *port);
    uint8_t (*opened)(void *ctx);
    uint8_t (*connected)(void *ctx);
    int (*disconnect)(void *ctx);
};

typedef struct _netio_t netio_t;

#endif