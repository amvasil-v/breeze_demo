/*
 * imget.h
 *
 *  Created on: 27 февр. 2021 г.
 *      Author: alexm
 */

#ifndef HTTPS_CLIENT_HTTPS_DOWNLOAD_H_
#define HTTPS_CLIENT_HTTPS_DOWNLOAD_H_

#include "https.h"
#include "netio.h"
#include "esp_uart.h"
#include "ext_storage.h"

int esp_init(esp_bridge_t *esp);
int https_download_image(ext_storage_t *storage, const char *url);

#endif /* HTTPS_CLIENT_HTTPS_DOWNLOAD_H_ */
