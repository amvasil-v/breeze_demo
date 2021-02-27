/*
 * imget.c
 *
 *  Created on: 27 февр. 2021 г.
 *      Author: alexm
 */

#include "https_download.h"

#include <stdio.h>

#include "main.h"
#include "wifi_cred.h"
#include "https.h"
#include "netio_esp.h"
#include "esp_utils.h"

static int https_download(HTTP_INFO *hi, netio_t *io, const char *url, ext_storage_t *storage);

int esp_init(esp_bridge_t *esp)
{

	int esp_ret;
	const char *ssid = ESP_WIFI_SSID;
	uint8_t do_connect = 0;

	esp_bridge_init(esp);
	printf("ESP: Waiting for boot\r\n");
	HAL_Delay(4000);

	if (esp_command_ate0(esp)) {
		printf("ESP: Failed to set ATE0\r\n");
		Error_Handler();
	} else {
		printf("ESP: set ATE0\r\n");
	}

	esp_ret = esp_command_cwjap_query(esp, ssid);
	if (esp_ret < 0) {
		Error_Handler();
	}
	if (esp_ret == 0) {
		printf("ESP: Connect to AP:\r\n");
		do_connect = 1;
	}
	if (esp_ret != 1) {
		printf("ESP: Invalid ESP state\r\n");
		Error_Handler();
	}

	if (do_connect) {
		esp_command_cwjap_connect(esp,ssid,ESP_WIFI_PASSWD);
		esp_ret = esp_command_cwjap_query(esp, ssid);
		if (esp_ret != 1) {
			printf("ESP: Failed to connect to AP\r\n");
			Error_Handler();
		}
	}

	printf("ESP: Connected to AP %s\r\n", ssid);

	return 0;
}

int https_download_image(ext_storage_t *storage, const char *url)
{
	esp_bridge_t esp;
	HTTP_INFO hi;

	if (esp_init(&esp)) {
		Error_Handler();
	}

	esp_debug_set_level(ESP_DBG_ERROR);

	netio_t *io = netio_esp_create(&esp);
	http_init(&hi, TRUE, io);

	printf("[HTTPS] Download image from %s\r\n", url);
	int ret = https_download(&hi, io, url, storage);
	if (!ret)
		printf("[HTTPS] Download complete\r\n");
	else
		printf("[HTTPS] Download failed\r\n");

	http_close(&hi);
	io->disconnect(io);
	return 0;
}

#define HTTPS_RANGED_SIZE   1024

#define HTTPS_PRINT_VERBOSE     1
#define HTTPS_PRINT_PROGRESS    0
#define HTTPS_PRINT_PROGRESS_BARS   30

#define HTTPS_SAVE_EXT_STORAGE	1

static int https_download(HTTP_INFO *hi, netio_t *io, const char *url, ext_storage_t *storage)
{
	char buf[HTTPS_RANGED_SIZE+1];
	size_t content_len = 0;
	size_t pos = 0;
#if HTTPS_PRINT_PROGRESS
	int curr_progress = 0;
	int progress_it;
#endif

	while (content_len == 0 || pos < content_len) {
		size_t range_end = pos + HTTPS_RANGED_SIZE - 1;
		size_t len = 0;
		if (content_len && range_end >= content_len)
			range_end = content_len - 1;

#if HTTPS_PRINT_VERBOSE
		printf("[HTTPS] Get ranged %d to %d from %d\r\n", pos, range_end, content_len);
#endif
		int ret = http_get_ranged(hi, url, buf, pos, range_end, &len, io);
		if ( len == 0) {
			printf("[HTTPS] Failed to get ranged HTTPS\r\n");
			return -1;
		}
		if (content_len == 0)
			content_len = len;
		else if (content_len != len) {
			printf("[HTTPS] Invalid content len %d\r\n", len);
			return -1;
		}

#if HTTPS_PRINT_PROGRESS
		int progress = (int)((float)(range_end + 1) / (float)content_len * HTTPS_PRINT_PROGRESS_BARS);
		if (curr_progress == 0) {
			printf("   [");
			for (progress_it = 0; progress_it < HTTPS_PRINT_PROGRESS_BARS - 1; progress_it++) {
				printf("-");
			}
			printf("]\n=>  ");
			curr_progress = 1;
		}
		if (progress > curr_progress) {
			for (progress_it = curr_progress; progress_it < progress; progress_it++) {
				printf(".");
			}
			curr_progress = progress;
		}
		if (progress == HTTPS_PRINT_PROGRESS_BARS) {
			printf(" Done!\n");
		}
		fflush(stdout);
#endif

#if HTTPS_SAVE_EXT_STORAGE
		size_t write_len = range_end - pos + 1;
		if (ext_storage_write(storage, buf, write_len)) {
			printf("Failed to save to ext storage\r\n");
			return -1;
		}
#endif

		pos = range_end + 1;
	}

	return 0;
}
