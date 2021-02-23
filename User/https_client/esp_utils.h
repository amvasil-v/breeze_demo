#ifndef ESP_UTILS_H
#define ESP_UTILS_H

#include <stddef.h>
#include <stdarg.h>
#include <stdint.h>

#define ESP_DBG_ERROR   1
#define ESP_DBG_WARN    2
#define ESP_DBG_INFO    3

const char *esp_find_substr(const char *hay, size_t hay_len, const char *needle, size_t needle_len);

int esp_debug(uint32_t priority, const char *format, ...);
void esp_debug_set_level(uint32_t lvl);

#endif 