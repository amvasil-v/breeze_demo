/*
 * image_hash.h
 *
 *  Created on: Mar 28, 2021
 *      Author: casca
 */

#ifndef INC_IMAGE_HASH_H_
#define INC_IMAGE_HASH_H_

#include <stddef.h>
#include <stdint.h>

void image_hash_init(void);
void image_hash_release(void);
void image_hash_feed(uint8_t *buf, size_t len);
uint8_t *image_hash_finalize(int buffer_num);
void image_hash_abort(void);
void image_hash_print(int buffer_num);
uint8_t image_hash_equal();
int image_hash_test(void);

#endif /* INC_IMAGE_HASH_H_ */
