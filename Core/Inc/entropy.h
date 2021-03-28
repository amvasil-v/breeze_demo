/*
 * entropy.h
 *
 *  Created on: Mar 28, 2021
 *      Author: casca
 */

#ifndef INC_ENTROPY_H_
#define INC_ENTROPY_H_

#include <stdint.h>

uint32_t entropy_get_hw_random(void);
int entropy_init(void);
uint32_t entropy_random(void);
void *entropy_get_ctr_drbg_context(void);

#endif /* INC_ENTROPY_H_ */
