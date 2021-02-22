#ifndef SCALE_STREAM_H
#define SCALE_STREAM_H

#include <stddef.h>
#include <stdint.h>

#define SCALE_STREAM_MAX_ROWS               5
#define SCALE_STREAM_DEFAULT_GRAY_LEVEL		180

struct _scale_stream_row_t
{
	size_t row;
	uint8_t *buf;
	uint8_t used;
};
typedef struct _scale_stream_row_t scale_stream_row_t;

enum _scale_type_t {
	SCALE_TYPE_FILL = 0,
	SCALE_TYPE_PRESERVE,
	SCALE_TYPE_COMBINED
};
typedef enum _scale_type_t scale_type_t;

struct _scale_stream_t
{
	size_t in_height;
	size_t in_width;
	size_t in_width_bytes;
	size_t out_width;
	size_t out_height;
	size_t display_width;
	size_t display_height;
	scale_stream_row_t in_rows[SCALE_STREAM_MAX_ROWS];
	float x_ratio;
	float y_ratio;
	size_t offset_x;
	size_t offset_y;

};
typedef struct _scale_stream_t scale_stream_t;

void scale_stream_init(scale_stream_t *ctx, size_t in_width, size_t in_height);
void scale_stream_scale_init(scale_stream_t *ctx, size_t display_width, size_t display_height, scale_type_t type);
int scale_stream_buffer_init(scale_stream_t *ctx, uint8_t *buf, size_t size);
uint8_t scale_stream_row_ready(scale_stream_t *ctx, size_t row_idx);
int scale_stream_feed(scale_stream_t *ctx, size_t x, size_t y, uint8_t value);
int scale_stream_process_out_row(scale_stream_t *ctx, size_t row, uint8_t *out_row_buf);
size_t scale_stream_check_row(scale_stream_t *ctx, size_t out_row);

#endif
