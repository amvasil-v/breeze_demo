/*
 * text_area.h
 *
 *  Created on: 4 мар. 2021 г.
 *      Author: alexm
 */

#ifndef TEXT_AREA_TEXT_AREA_H_
#define TEXT_AREA_TEXT_AREA_H_

#include <stddef.h>
#include <stdint.h>

typedef void (*text_area_draw_cb_t)(void *,size_t, size_t, uint8_t);

struct _text_area_t {
	size_t width;
	size_t height;
	void *draw_ctx;
	uint8_t transparent;
	text_area_draw_cb_t draw_cb;
	uint8_t fill;
	uint8_t border;
	size_t offset_x;
	size_t offset_y;
};
typedef struct _text_area_t text_area_t;

void text_area_init(text_area_t *area, size_t width, size_t height);
void text_area_set_transparent(text_area_t *area, uint8_t t);
void text_area_setup_draw(text_area_t *area, text_area_draw_cb_t draw_cb, void *ctx);
int text_area_render(text_area_t *area, const char *str);

#endif /* TEXT_AREA_TEXT_AREA_H_ */
