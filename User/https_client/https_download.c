/*
 * imget.c
 *
 *  Created on: 27 февр. 2021 г.
 *      Author: alexm
 */

#include "https_download.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "main.h"
#include "wifi_cred.h"
#include "https.h"
#include "netio_esp.h"
#include "esp_utils.h"
#include "json_parser.h"

static int https_download(HTTP_INFO *hi, netio_t *io, const char *url, ext_storage_t *storage);
static int https_select_image_url(HTTP_INFO *hi, netio_t *io, image_data_t *image_data);

int esp_init(esp_bridge_t *esp)
{

	int esp_ret;
	const char *ssid = ESP_WIFI_SSID;
	uint8_t do_connect = 0;

	esp_bridge_init(esp);
	printf("ESP: Waiting for boot\r\n");
	HAL_Delay(1000);

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

#define HTTPS_IMAGE_URL_BUF_SIZE	256

int https_download_image(ext_storage_t *storage, image_data_t *image_data)
{
	esp_bridge_t esp;
	HTTP_INFO hi;
	static const uint32_t image_ext_addr = 0x0;

	if (esp_init(&esp)) {
		Error_Handler();
	}

	esp_debug_set_level(ESP_DBG_ERROR);

	netio_t *io = netio_esp_create(&esp);
	http_init(&hi, TRUE, io);

	printf("[HTTPS] Select image URL\r\n");
	if (https_select_image_url(&hi, io, image_data)) {
		Error_Handler();
	}

	printf("[HTTPS] Image size %u, prepare storage\r\n", image_data->size);
	if (ext_storage_prepare(storage, image_ext_addr, image_data->size)) {
		Error_Handler();
	}

	printf("[HTTPS] Download image from %s\r\n", image_data->url);
	int ret = https_download(&hi, io, image_data->url, storage);
	if (!ret)
		printf("[HTTPS] Download complete\r\n");
	else {
		printf("[HTTPS] Download failed\r\n");
		Error_Handler();
	}

	http_close(&hi);
	io->disconnect(io);
	return 0;
}

#define HTTPS_RANGED_SIZE   1024

#define HTTPS_PRINT_VERBOSE     1
#define HTTPS_PRINT_PROGRESS    0
#define HTTPS_PRINT_PROGRESS_BARS   30

#define HTTPS_SAVE_EXT_STORAGE	1

static char https_buf[HTTPS_RANGED_SIZE + 1];

static int https_download(HTTP_INFO *hi, netio_t *io, const char *url, ext_storage_t *storage)
{
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
		int ret = http_get_ranged(hi, (char *)url, https_buf, pos, range_end, &len, io);
		if ( len == 0 || (ret != 200 && ret != 206)) {
			printf("[HTTPS] Failed to get ranged HTTPS\r\n");
			return -1;
		}
		if (content_len == 0)
			content_len = len;
		else if (content_len != len) {
			printf("[HTTPS] Invalid content len %d\r\n", len);
			return -1;
		}
		led_blink(1);

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
		if (ext_storage_write(storage, (uint8_t *)https_buf, write_len)) {
			printf("Failed to save to ext storage\r\n");
			return -1;
		}
#endif

		pos = range_end + 1;
	}

	return 0;
}


static int https_select_image_url(HTTP_INFO *hi, netio_t *io, image_data_t *image_data)
{
	static const char *main_url = "https://xkcd.com/info.0.json";
	int content_len = 0;
	static char tmp[IMAGE_URL_SIZE];

	led_set(0, 0);
	led_set(1, 0);

	int ret = http_get(hi, (char *)main_url, https_buf, sizeof(https_buf), io);
	printf("return code: %d \r\n", ret);
	led_set(1, 1);

	if (ret != 200)
	{
		printf("HTTPS request failed\r\n");
		Error_Handler();
	}

	int num_comics = -1;
	if (json_parser_get_number(https_buf, "num", &num_comics)) {
		printf("Failed to parse json\r\n");
		printf("%s\r\n", https_buf);
		Error_Handler();
	}
	if (num_comics <= 0) {
		printf("Invalid num_comics %d\r\n", num_comics);
		Error_Handler();
	}
	printf("Num comics: %d\r\n", num_comics);

	srand(HAL_GetTick());
	while (1) {
		int selected = rand() % num_comics + 1;
		printf("Selected: %d\r\n", selected);

		sprintf(tmp, "https://xkcd.com/%d/info.0.json", selected);

		led_set(1, 1);
		ret = http_get(hi, tmp, https_buf, sizeof(https_buf), io);
		led_set(1, 0);
		printf("return code: %d \r\n", ret);
		if (ret != 200)
		{
			printf("HTTPS request failed\r\n");
			Error_Handler();
		}

		if (json_parser_get_string(https_buf, "img", tmp, sizeof(tmp))) {
			printf("Failed to get img URL from json\r\n");
			HAL_Delay(500);
			continue;
		}
		const char *image_url = tmp;
		printf("Image URL: %s\r\n", image_url);
		if (strlen(image_url) >= IMAGE_URL_SIZE - 1) {
			printf("Image URL doesn't fit into buffer\r\n");
			Error_Handler();
		}
		strlcpy(image_data->url, image_url, IMAGE_URL_SIZE);

		if (json_parser_get_string(https_buf, "safe_title", tmp, sizeof(tmp))) {
			printf("Failed to get img title from json\r\n");
			HAL_Delay(500);
			continue;
		}
		const char *image_title = tmp;
		printf("Image title: %s\r\n", image_title);
		strlcpy(image_data->title, image_title, IMAGE_URL_SIZE);

		io->disconnect(io);

		memset(https_buf, 0, sizeof(https_buf));
		led_set(1, 0);
		HAL_Delay(200);
		printf("Get image HTTPS header\r\n");
		ret = http_get_header(hi, image_data->url, https_buf, sizeof(https_buf), io);
		printf("return code: %d \r\n", ret);
		if (ret != 200)
		{
			printf("HTTPS request failed\r\n");
			Error_Handler();
		}

		content_len = hi->response.content_length;
		printf("Image size: %d\r\n", content_len);
		printf("Response size: %d\r\n", strlen(https_buf));
		led_set(1, 1);

		if (content_len > PNG_MAX_SIZE) {
			printf("Image too big\r\n");
			io->disconnect(io);
			HAL_Delay(1000);
		} else {
			break;
		}
	}

	image_data->size = content_len;
	return 0;
}
