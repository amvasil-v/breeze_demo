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
extern RTC_HandleTypeDef hrtc;

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

void system_go_standby(uint8_t min_bcd, uint8_t sec_bcd)
{
	HAL_RTC_DeactivateAlarm(&hrtc, RTC_ALARM_A);

	RTC_AlarmTypeDef sAlarm = {0};

	sAlarm.AlarmTime.Hours = 0x0;
	sAlarm.AlarmTime.Minutes = min_bcd;
	sAlarm.AlarmTime.Seconds = sec_bcd;
	sAlarm.AlarmTime.SubSeconds = 0x0;
	sAlarm.AlarmTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
	sAlarm.AlarmTime.StoreOperation = RTC_STOREOPERATION_RESET;
	sAlarm.AlarmMask = RTC_ALARMMASK_DATEWEEKDAY|RTC_ALARMMASK_HOURS;
	if (!min_bcd) {
		sAlarm.AlarmMask |= RTC_ALARMMASK_MINUTES;
	}
	sAlarm.AlarmSubSecondMask = RTC_ALARMSUBSECONDMASK_ALL;
	sAlarm.AlarmDateWeekDaySel = RTC_ALARMDATEWEEKDAYSEL_DATE;
	sAlarm.AlarmDateWeekDay = 0x1;
	sAlarm.Alarm = RTC_ALARM_A;
	if (HAL_RTC_SetAlarm_IT(&hrtc, &sAlarm, RTC_FORMAT_BCD) != HAL_OK)
	{
		Error_Handler();
	}

	printf("Waiting for RTC alarm in %u:%u\r\n", min_bcd, sec_bcd);
	HAL_PWR_EnterSTANDBYMode();

	HAL_NVIC_SystemReset();
}



