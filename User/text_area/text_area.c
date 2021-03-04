/*
 * text_area.c
 *
 *  Created on: 4 мар. 2021 г.
 *      Author: alexm
 */

#include "text_area.h"

#include "b_code_b_regular_16_font.h"

#define font_lookup b_code_b_regular_16_font_lookup
#define font_pixels b_code_b_regular_16_font_pixels

void text_area_init(text_area_t *area, size_t width, size_t height)
{
	area->width = width;
	area->height = height;
	area->transparent = 1;
	area->draw_cb = NULL;
	area->draw_ctx = NULL;
	area->fill = 1;
	area->border = 1;
	area->offset_x = 2;
	area->offset_y = 0;
}

void text_area_set_transparent(text_area_t *area, uint8_t t)
{
	area->transparent = t;
}

void text_area_setup_draw(text_area_t *area, text_area_draw_cb_t draw_cb,  void *ctx)
{
	area->draw_cb = draw_cb;
	area->draw_ctx = ctx;
}

static void text_area_fill(text_area_t *area)
{
	if (!area->fill && !area->border) {
		return;
	}
	for (size_t y = 0; y < area->height; y++) {
		for (size_t x = 0; x < area->width; x++) {
			if ((x == 0 || y == 0 || x == area->width - 1 || y == area->height - 1) &&
				area->border) {
				area->draw_cb(area->draw_ctx, x, y, !area->transparent);
			} else {
				area->draw_cb(area->draw_ctx, x, y, area->transparent);
			}
		}
	}
}

int text_area_render(text_area_t *area, const char *str)
{
	char c = *str;
	size_t pos_x = area->offset_x;
	size_t pos_y = area->offset_y;
	text_area_fill(area);
	while (c) {
		if (c == '\n') {
			pos_y += font_lookup[0].advance;
			c = *(++str);
			continue;
		}
		const struct font_char font_c = font_lookup[(int)c];
		for (int y = 0; y < font_c.h; ++y) {
			for (int x = 0; x < font_c.w; ++x) {
				uint8_t v = font_pixels[font_c.offset + x + y * font_c.w];
				if ((!v) != area->transparent) {
					size_t px = pos_x + x + font_c.left;
					size_t py = pos_y + y + font_c.top;
					if (px < area->width && py < area->height) {
						area->draw_cb(area->draw_ctx, px, py, !area->transparent);
					}
				}
			}
		}
		pos_x += font_c.advance - 3;
		c = *(++str);
	}
	return 0;
}

