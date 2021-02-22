/*
 * display.h
 *
 *  Created on: 22 февр. 2021 г.
 *      Author: alexm
 */

#ifndef INC_DISPLAY_H_
#define INC_DISPLAY_H_

#include "main.h"

#define DISPLAY_WIDTH		EPD_7IN5BC_WIDTH
#define DISPLAY_HEIGHT		EPD_7IN5BC_HEIGHT
#define DISPLAY_BUFFER_SIZE (DISPLAY_WIDTH * DISPLAY_HEIGHT / 8)

int display_init(void);
int display_chess(void);
int display_diagonal(void);
int display_clear(void);
int display_deinit(void);

int load_png_image(uint8_t *addr, size_t size);
int display_png_image(void);


#endif /* INC_DISPLAY_H_ */
