/*
 * sys_control.h
 *
 *  Created on: 5 мар. 2021 г.
 *      Author: alexm
 */

#ifndef INC_SYS_CONTROL_H_
#define INC_SYS_CONTROL_H_

#include <stddef.h>
#include <stdint.h>

void system_error_handler(void);
void system_wait_and_reset(uint32_t wait, uint32_t period);
void system_go_standby(uint8_t min_bcd, uint8_t sec_bcd);

#endif /* INC_SYS_CONTROL_H_ */
