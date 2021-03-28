/*
 * entropy.c
 *
 *  Created on: Mar 28, 2021
 *      Author: casca
 */

#include "entropy.h"

#include "main.h"
#include <string.h>
#include <stdio.h>

#include "sys_control.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"

extern ADC_HandleTypeDef hadc1;

static mbedtls_entropy_context entropy;
static mbedtls_ctr_drbg_context ctr_drbg;

//#define SYS_ENTROPY_PRINT

uint32_t entropy_get_hw_random(void)
{
	uint32_t num = 0;
	uint32_t val = 0;

	for (int i = 0; i < sizeof(num) * 8; i++) {
		while (1) {
			HAL_ADC_Start(&hadc1);
			if (HAL_ADC_PollForConversion(&hadc1, 100) != HAL_OK) {
				printf("ADC failed\r\n");
				Error_Handler();
			}
			val = HAL_ADC_GetValue(&hadc1);
			if (val == 0 || val == ((1 << 12) - 1)) {
				/* saturation protection */
				continue;
			} else {
				break;
			}
		}
		num ^= val << i;
	}
#ifdef SYS_ENTROPY_PRINT
	printf("HW entropy: 0x%08lx\r\n", num);
#endif
	return num;
}

static int entropy_callback(void *data, unsigned char *output, size_t len, size_t *olen)
{
	UNUSED(data);
	uint32_t num = entropy_get_hw_random();
	size_t to_copy = sizeof(num) > len ? len : sizeof(num);
	memcpy(output, &num, to_copy);
	*olen = to_copy;
	return 0;
}

int entropy_init(void)
{
	mbedtls_entropy_init(&entropy);

	if (mbedtls_entropy_add_source(&entropy, entropy_callback, NULL, sizeof(uint32_t), MBEDTLS_ENTROPY_SOURCE_STRONG )) {
		printf("Entropy add failed\r\n");
		return -1;
	}

	char *personalization = "breeze";
	mbedtls_ctr_drbg_init(&ctr_drbg);
	if (mbedtls_ctr_drbg_seed(&ctr_drbg , mbedtls_entropy_func, &entropy,
            (const unsigned char *) personalization,
            strlen( personalization ))) {
		printf("Failed to init ctr_drbg\r\n");
		return -1;
	}
	return 0;
}

uint32_t entropy_random(void)
{
	uint32_t val;

	if (mbedtls_ctr_drbg_random(&ctr_drbg, (unsigned char *)&val, sizeof(val))) {
		printf("Failed to get random value\r\n");
		Error_Handler();
	}

	return val;
}

void *entropy_get_ctr_drbg_context(void)
{
	return (void *)&ctr_drbg;
}


