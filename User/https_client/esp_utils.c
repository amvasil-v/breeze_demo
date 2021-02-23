#include "esp_utils.h"

#include <stdio.h>

static uint32_t esp_debug_level = 0;

const char *esp_find_substr(const char *hay, size_t hay_len, const char *needle, size_t needle_len)
{
    int i,j;
    int found;

    if (hay_len == 0 || needle_len == 0 || hay_len < needle_len)
        return NULL;

    for (i = 0; i < hay_len - needle_len + 1; i++) {
        if (hay[i] == needle[0]) {
            found = 1;
            for (j = 1; j < needle_len; j++) {
                if (hay[i+j] != needle[j]) {
                    found = 0;
                    break;
                }
            }
            if (found)
                return &hay[i];
        }
    }

    return NULL;
}

int esp_debug(uint32_t priority, const char *format, ...)
{
	int ret;
    va_list args;
    va_start(args, format);

    if(priority <= esp_debug_level)
            ret = vprintf(format, args);

    va_end(args);
    return ret;
}

void esp_debug_set_level(uint32_t lvl)
{
    esp_debug_level = lvl;
}
