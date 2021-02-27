/*
 * ext_storage.c
 *
 *  Created on: 27 февр. 2021 г.
 *      Author: alexm
 */

#include "ext_storage.h"

#include <stdio.h>
#include <string.h>

#include "main.h"
#include "qspi_driver.h"

int ext_storage_init(ext_storage_t *ext)
{
	if (QSPI_init_device()) {
		printf("Failed to init QSPI flash\r\n");
		return -1;
	}
	return 0;
}

int ext_storage_test(ext_storage_t *ext)
{
	//#define PROGRAM_IMAGE_QSPI
#ifdef PROGRAM_IMAGE_QSPI
	const size_t qspi_addr = 0;

	if (QSPI_area_erase(qspi_addr, PNG_IMAGE_SIZE)) {
		printf("Failed to erase QSPI flash\r\n");
		Error_Handler();
	}
	HAL_Delay(100);
	size_t bytes_written = 0;
	const size_t write_chunk = QSPI_PAGE_SIZE;
	static uint8_t write_buf[QSPI_PAGE_SIZE];
	while (bytes_written < PNG_IMAGE_SIZE) {
		size_t to_write = PNG_IMAGE_SIZE - bytes_written;
		if (to_write > write_chunk) {
			to_write = write_chunk;
		}
		memcpy(write_buf, (uint8_t *)PNG_IMAGE_ADDRESS + bytes_written, to_write);
		if (QSPI_write_page(write_buf, to_write, qspi_addr + bytes_written)) {
			Error_Handler();
		}
		bytes_written += to_write;
	}
	printf("Written PNG image to QSPI flash\r\n");
#endif
	return 0;
}

int ext_storage_prepare(ext_storage_t *ext, uint32_t store_addr, size_t size)
{
	ext->start_address = store_addr;
	ext->stored_size = size;
	ext->bytes_written = 0;

	if (QSPI_area_erase(store_addr, size)) {
		printf("Failed to erase QSPI flash\r\n");
		return -1;
	}
	HAL_Delay(100);
	return 0;
}

int ext_storage_write(ext_storage_t *ext, uint8_t *buf, size_t len)
{
	if (!ext->stored_size) {
		printf("Ext storage not configured\r\n");
		return -1;
	}

	size_t written = 0;
	const size_t write_chunk = QSPI_PAGE_SIZE;
	static uint8_t write_buf[QSPI_PAGE_SIZE];
	while (written < len) {
		size_t to_write = len - written;
		if (to_write > write_chunk) {
			to_write = write_chunk;
		}
		memcpy(write_buf, &buf[written], to_write);
		uint32_t dst = ext->start_address + ext->bytes_written;
		if (QSPI_write_page(write_buf, to_write, dst)) {
			printf("Failed to write to 0x%08lx\r\n", dst);
			return -1;
		}
		ext->bytes_written += to_write;
		written += to_write;
	}
	HAL_Delay(100);
	return 0;
}

int ext_storage_read(ext_storage_t *ext, uint8_t *buf, size_t len, uint32_t src_addr)
{
	uint32_t bytes_read = 0;
	const size_t read_chunk = 128;
	while (bytes_read < len) {
		size_t to_read = len - bytes_read;
		if (to_read > read_chunk) {
			to_read = read_chunk;
		}
		if (QSPI_read_page(&buf[bytes_read], to_read, src_addr + bytes_read)) {
			printf("Failed to read from QSPI 0x%08lx\r\n", src_addr);
			return -1;
		}
		bytes_read += to_read;
	}
	return 0;
}

#define EXT_STORAGE_RW_TEST_CHUNK			1024
#define EXT_STORAGE_RW_TEST_LENGTH			10000
#define EXT_STORAGE_TEST_PATTERN_INIT		0x55
#define EXT_STORAGE_TEST_PATTERN_INC		0x03

int ext_storage_test_read_write(void)
{
	uint32_t addr = 0;
	static uint8_t buf[EXT_STORAGE_RW_TEST_CHUNK];
	size_t written = 0;
	size_t read = 0;
	uint8_t pattern = EXT_STORAGE_TEST_PATTERN_INIT;
	ext_storage_t ext;

	if (ext_storage_init(&ext)) {
		Error_Handler();
	}

	if (ext_storage_prepare(&ext, addr, EXT_STORAGE_RW_TEST_LENGTH)) {
		Error_Handler();
	}

	printf("Write test pattern to ext storage, length %d\r\n", EXT_STORAGE_RW_TEST_LENGTH);
	while (written < EXT_STORAGE_RW_TEST_LENGTH) {
		for (int i = 0; i < EXT_STORAGE_RW_TEST_CHUNK; i++) {
			buf[i] = pattern;
			pattern += EXT_STORAGE_TEST_PATTERN_INC;
		}
		size_t to_write = EXT_STORAGE_RW_TEST_LENGTH - written;
		if (to_write > EXT_STORAGE_RW_TEST_CHUNK) {
			to_write = EXT_STORAGE_RW_TEST_CHUNK;
		}
		if (ext_storage_write(&ext, buf, to_write)) {
			Error_Handler();
		}
		written += to_write;
		led_blink(0);
	}
	printf("Pattern written\r\n");
	led_set(0, 0);

	printf("Verifying...\r\n");
	pattern = EXT_STORAGE_TEST_PATTERN_INIT;
	while (read < EXT_STORAGE_RW_TEST_LENGTH) {
		size_t to_read = EXT_STORAGE_RW_TEST_LENGTH - read;
		if (to_read > EXT_STORAGE_RW_TEST_CHUNK) {
			to_read = EXT_STORAGE_RW_TEST_CHUNK;
		}
		if (ext_storage_read(&ext, buf, to_read, addr + read)) {
			Error_Handler();
		}
		for (int i = 0; i < to_read; i++) {
			if (buf[i] != pattern) {
				printf("Pattern verify failed at 0x%08lx: expect 0x%02x, read 0x%02x\r\n",
						addr + read, pattern, buf[i]);
				return -1;
			}
			pattern += EXT_STORAGE_TEST_PATTERN_INC;
		}
		read += to_read;
		led_blink(1);
	}
	led_set(1, 0);
	printf("Pattern verified\r\n");
	return 0;

}
