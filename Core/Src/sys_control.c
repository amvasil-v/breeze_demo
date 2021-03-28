/*
 * sys_control.c
 *
 *  Created on: 5 мар. 2021 г.
 *      Author: alexm
 */

#include "sys_control.h"

#include <stdio.h>

#include "main.h"
#include "core_cm4.h"

extern TIM_HandleTypeDef htim11;
extern UART_HandleTypeDef huart1;

void system_error_handler (void)
{
	static uint8_t reset_on_error = 1;
	led_set(0, 0);
	led_set(1, 0);
	while (1) {
		for (int j = 0; j < 10; j++) {
			led_blink(0);
			led_blink(1);
			for (int i = 0; i < 2000000; i++) {
				__NOP();
			}
		}
		if (reset_on_error) {
			HAL_NVIC_SystemReset();
		}
	}
}

void system_wait_and_reset(uint32_t wait, uint32_t period)
{
	printf("Waiting %lu seconds\r\n", wait);
	uint32_t tim_period = 3600 * period - 360 - 1;

	HAL_UART_Abort(&huart1);
	HAL_UART_DeInit(&huart1);

	for (int i = 0; i < wait / period; i++) {
		led_set(1, 1);
		HAL_Delay(100);
		led_set(1, 0);

		htim11.Instance->CNT = 0;
		htim11.Instance->ARR = tim_period;
		HAL_TIM_Base_Start_IT(&htim11);

		/* disable SYSTICK irq */
		SysTick->CTRL &= ~SysTick_CTRL_TICKINT_Msk;
		HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);

		SysTick->CTRL |= SysTick_CTRL_TICKINT_Msk;
		HAL_TIM_Base_Stop_IT(&htim11);
		//Error_Handler();
	}

	printf("System reset\r\n");
	HAL_Delay(300);
	HAL_NVIC_SystemReset();
}



