/*
 * ext_storage.h
 *
 *  Created on: 27 февр. 2021 г.
 *      Author: alexm
 */

#ifndef INC_EXT_STORAGE_H_
#define INC_EXT_STORAGE_H_

#include <stdint.h>
#include <stddef.h>

struct _ext_storage_t {
	size_t stored_size;
	uint32_t start_address;
	uint32_t bytes_written;
};
typedef struct _ext_storage_t ext_storage_t;

int ext_storage_init(ext_storage_t *ext);
int ext_storage_prepare(ext_storage_t *ext, uint32_t store_addr, size_t size);
int ext_storage_write(ext_storage_t *ext, uint8_t *buf, size_t len);
int ext_storage_read(ext_storage_t *ext, uint8_t *buf, size_t len, uint32_t src_addr);
int ext_storage_test_read_write();

#endif /* INC_EXT_STORAGE_H_ */
