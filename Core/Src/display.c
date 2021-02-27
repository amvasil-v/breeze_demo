/*
 * display.c
 *
 *  Created on: 22 февр. 2021 г.
 *      Author: alexm
 */

#include "display.h"

#include <stdio.h>
#include <string.h>

#include "EPD_Test.h"
#include "EPD_7in5bc.h"
#include "pngle/pngle.h"
#include "scale_stream.h"


static size_t png_image_width;
static size_t png_image_height;
static uint8_t display_buffer[DISPLAY_BUFFER_SIZE];
static uint8_t tmp_buf[800 * SCALE_STREAM_MAX_ROWS];
static size_t out_width_bytes;
static scale_stream_t scale_ctx;
static int draw_error = 0;
static int draw_curr_row = 0;

int display_init(void)
{
	printf("Display init\r\n");
	DEV_Module_Init();
	EPD_7IN5BC_Init();
	EPD_7IN5BC_Clear();
	DEV_Delay_ms(100);
	printf("Display init done\r\n");
	return 0;
}

static inline int display_put_pixel(size_t x, size_t y, uint8_t value)
{
	if (x >= DISPLAY_WIDTH || y >= DISPLAY_HEIGHT)
		return -1;

	size_t pos = (x + y * DISPLAY_WIDTH) / 8;
	if (value) {
		display_buffer[pos] |= 0x80 >> (x % 8);
	} else {
		display_buffer[pos] &= ~(0x80 >> (x % 8));
	}
	return 0;
}

int display_chess(void)
{
	printf("Display chess image\r\n");

	memset(display_buffer, 0, sizeof(display_buffer));

	for (int x = 0; x < DISPLAY_WIDTH; x++) {
		for (int y = 0; y < DISPLAY_HEIGHT; y++) {
			uint8_t pixel = ((x / 20) + (y / 20)) % 2;
			display_put_pixel(x, y, pixel);
		}
	}

	EPD_7IN5BC_Display((const UBYTE *)display_buffer, NULL);

	printf("Drawing done\r\n");
	return 0;
}

int display_diagonal(void)
{
	printf("Display diagonal line\r\n");
	memset(display_buffer, 0xff, sizeof(display_buffer));
	for (int x = 0; x < DISPLAY_HEIGHT; x++) {
		display_put_pixel(x, x, 0);
	}
	EPD_7IN5BC_Display((const UBYTE *)display_buffer, NULL);
	printf("Drawing done\r\n");
	return 0;
}

int display_clear(void)
{
	printf("Display clear\r\n");
	EPD_7IN5BC_Clear();
	printf("Clear done\r\n");
	return 0;
}

int display_deinit(void)
{
	EPD_7IN5BC_Sleep();
	DEV_Module_Exit();
	return 0;
}

static void picture_init(pngle_t *pngle, uint32_t w, uint32_t h)
{
	printf("Create %lu by %lu picture\r\n", w, h);
	png_image_width = w;
	png_image_height = h;
	draw_error = 0;
	out_width_bytes = (DISPLAY_WIDTH + 7) / 8;
	memset(display_buffer, 0xFF, sizeof(display_buffer));
	scale_stream_init(&scale_ctx, w, h);
	scale_stream_scale_init(&scale_ctx, DISPLAY_WIDTH, DISPLAY_HEIGHT, SCALE_TYPE_PRESERVE);
	if (scale_stream_buffer_init(&scale_ctx, tmp_buf, sizeof(tmp_buf))) {
		printf("Failed to init temp buffer\r\n");
		draw_error = -2;
	}
	draw_curr_row = 0;
}

static void picture_draw(pngle_t *pngle, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint8_t rgba[4])
{
	uint32_t val = 0;
	for (int i = 0; i < 3; i++)
		val += rgba[i];
	if (draw_error)
		return;
	if (scale_stream_feed(&scale_ctx, x, y, val / 3)) {
		printf("Feed error at %lu %lu\r\n", x, y);
		draw_error = -1;
		return;
	}

	if (x == scale_ctx.in_width - 1) {
		while (1) {
			size_t check_row = scale_stream_check_row(&scale_ctx, draw_curr_row);
			if (check_row > y) {
				break;
			}
			if (scale_stream_process_out_row(&scale_ctx, draw_curr_row,
					&display_buffer[(draw_curr_row + scale_ctx.offset_y) * out_width_bytes])) {
				printf("Draw error at %lu %lu\r\n", x, y);
				draw_error = -1;
				return;
			}
			draw_curr_row++;
		}
	}
}

static void picture_done(pngle_t *pngle)
{
	printf("Done loading png %ux%u image\r\n", scale_ctx.out_width, scale_ctx.out_height);
}

int load_png_image(uint8_t *addr, size_t size)
{
	pngle_t *pngle = pngle_new();
	int res = 0;

	pngle_set_draw_callback(pngle, picture_draw);
	pngle_set_init_callback(pngle, picture_init);
	pngle_set_done_callback(pngle, picture_done);

	printf("Loading PNG image\r\n");
	int fed = pngle_feed(pngle, addr, size);
	if (fed == size) {
		printf("Image loaded\r\n");
	} else {
		printf("Image load failed with res %d\r\n", fed);
		res = -1;
	}

	free(pngle);
	return res;
}

static pngle_t *pngle_inst = NULL;

int load_png_image_init(void)
{
	pngle_inst = pngle_new();
	pngle_set_draw_callback(pngle_inst, picture_draw);
	pngle_set_init_callback(pngle_inst, picture_init);
	pngle_set_done_callback(pngle_inst, picture_done);
	return 0;
}

int load_png_image_feed(uint8_t *buf, size_t size)
{
	if (!pngle_inst) {
		return -1;
	}
	int fed = pngle_feed(pngle_inst, buf, size);
	if (fed != size) {
		printf("Failed to feed png image\r\n");
		return -1;
	}
	return 0;
}

int load_png_image_release(void)
{
	free(pngle_inst);
	pngle_inst = NULL;
	return 0;
}

int display_png_image(void)
{
	printf("Display image\r\n");
	EPD_7IN5BC_Display((const UBYTE *)display_buffer, NULL);
	printf("Drawing done\r\n");
	return 0;
}
