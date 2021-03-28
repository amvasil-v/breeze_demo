/*
 * image_hash.c
 *
 *  Created on: Mar 28, 2021
 *      Author: casca
 */

#include "image_hash.h"

#include <stdio.h>
#include <string.h>

#include "main.h"
#include "mbedtls/sha256.h"

#define HASH_LEN_BYTES	(256 / 8)

static mbedtls_sha256_context hash_ctx;
static uint8_t is_computing;
static uint8_t hash[2][HASH_LEN_BYTES];

void image_hash_init(void)
{
	mbedtls_sha256_init(&hash_ctx);
	is_computing = 0;
	memset(hash, 0, HASH_LEN_BYTES);
}

void image_hash_release(void)
{
	mbedtls_sha256_free(&hash_ctx);
	is_computing = 0;
}

void image_hash_feed(uint8_t *buf, size_t len)
{
	if (!is_computing) {
		mbedtls_sha256_starts(&hash_ctx, 0);
		is_computing = 1;
	}
	mbedtls_sha256_update(&hash_ctx, buf, len);
}

uint8_t *image_hash_finalize(int buffer_num)
{
	if (!is_computing || buffer_num < 0 || buffer_num > 1) {
		return NULL;
	}
	mbedtls_sha256_finish(&hash_ctx, hash[buffer_num]);
	is_computing = 0;
	return hash[buffer_num];
}

void image_hash_abort(void)
{
	is_computing = 0;
}

void image_hash_print(int buffer_num)
{
	printf("hash %d: ", buffer_num);
	for (int i = 0; i < HASH_LEN_BYTES; i++) {
		printf("%02x", hash[0][i]);
	}
	printf("\r\n");
}

uint8_t image_hash_equal()
{
#ifdef PRINT_HASH_COMPARE
	image_hash_print(0);
	image_hash_print(1);
#endif
	return !memcmp(hash[0], hash[1], HASH_LEN_BYTES);
}

int image_hash_test(void)
{
	uint8_t buf[100];
	memset(buf, 0xAA, sizeof(buf));
	image_hash_init();
	image_hash_feed(buf, 100);
	image_hash_finalize(0);

	image_hash_feed(&buf[0], 50);
	image_hash_feed(&buf[1], 50);
	image_hash_finalize(1);

	if (image_hash_equal()) {
		printf("Image hash test part 1 complete\r\n");
	} else {
		printf("Image hash test part 1 failed\r\n");
		Error_Handler();
	}

	buf[49] = 0xAB;
	image_hash_feed(buf, 100);
	image_hash_finalize(1);

	if (!image_hash_equal()) {
		printf("Image hash test part 2 complete\r\n");
	} else {
		printf("Image hash test part 2 failed\r\n");
		Error_Handler();
	}

	image_hash_release();
	return 0;
}






