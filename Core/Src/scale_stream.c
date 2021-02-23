#include "scale_stream.h"

static void calculate_scale_preserve(scale_stream_t *ctx);

void scale_stream_init(scale_stream_t *ctx, size_t in_width, size_t in_height)
{
    ctx->in_width = in_width;
    ctx->in_height = in_height;
    for (int i = 0; i < SCALE_STREAM_MAX_ROWS; i++) {
        ctx->in_rows[i].row = 0;
        ctx->in_rows[i].used = 0;
    }
    ctx->in_width_bytes = ctx->in_width;
}

void scale_stream_scale_init(scale_stream_t *ctx, size_t display_width, size_t display_height, scale_type_t type)
{
    ctx->display_width = display_width;
    ctx->display_height = display_height;

    switch (type)
    {
    case SCALE_TYPE_PRESERVE:
        calculate_scale_preserve(ctx);
        break;
    case SCALE_TYPE_FILL: /* fallthru */
    default:
        ctx->out_width = display_width;
        ctx->out_height = display_height;
        break;
    }

    ctx->x_ratio = ((float)(ctx->in_width - 1)) / ctx->out_width;
    ctx->y_ratio = ((float)(ctx->in_height - 1)) / ctx->out_height;

    ctx->offset_x = (ctx->display_width - ctx->out_width) / 2;
    ctx->offset_y = (ctx->display_height - ctx->out_height) / 2;
}

int scale_stream_buffer_init(scale_stream_t *ctx, uint8_t *buf, size_t size)
{
    if (size < ctx->in_width_bytes * SCALE_STREAM_MAX_ROWS) {
        return -1;
    }
    for (int i = 0; i < SCALE_STREAM_MAX_ROWS; i++) {
        ctx->in_rows[i].buf = &buf[i * ctx->in_width_bytes];
    }
    return 0;
}

uint8_t scale_stream_row_ready(scale_stream_t *ctx, size_t row_idx)
{
    size_t required_in_row = (size_t)(ctx->y_ratio * row_idx);
    uint8_t found = 0;

    for (int i = 0; i < SCALE_STREAM_MAX_ROWS; i++) {
        if (ctx->in_rows[i].buf && ctx->in_rows[i].row == required_in_row) {
            found = 1;
        }
    }
    return found;
}

static int find_row_idx(scale_stream_t *ctx, size_t row)
{
    size_t oldest_row = SIZE_MAX;
    int idx = -1;

    for (int i = 0; i < SCALE_STREAM_MAX_ROWS; i++) {
        if (ctx->in_rows[i].row == row && ctx->in_rows[i].used) {
            return i;
        }
        if (!ctx->in_rows[i].used) {
            idx = i;
            ctx->in_rows[idx].used = 1;
            break;
        }
        if (ctx->in_rows[i].row <= oldest_row) {
            idx = i;
            oldest_row = ctx->in_rows[i].row;
        }
    }

    ctx->in_rows[idx].row = row;
    return idx;
}

#define IN_PIXEL_MASK(POS)      (1 << (POS % 8))
#define IN_PIXEL_IDX(POS)       (POS / 8)

static inline void put_pixel_row_in(uint8_t *buf, size_t x, uint8_t val)
{
    buf[x] = val;
}

static inline uint8_t get_pixel_row_in(uint8_t *buf, size_t x)
{
    return buf[x];
}

int scale_stream_feed(scale_stream_t *ctx, size_t x, size_t y, uint8_t value)
{
    int row = find_row_idx(ctx, y);

    if (row < 0)
    {
        return row;
    }
    if (x >= ctx->in_width || y >= ctx->in_height)
        return -1;
    put_pixel_row_in(ctx->in_rows[row].buf, x, value);
    return 0;
}

#define OUT_PIXEL_MASK(POS)      (0x80 >> (POS % 8))
#define OUT_PIXEL_IDX(POS)       (POS / 8)

static inline void set_out_row_pixel(scale_stream_t *ctx, uint8_t *buf, size_t x, uint8_t val)
{
    size_t pos = x + ctx->offset_x;
    if (val)
        buf[OUT_PIXEL_IDX(pos)] |= OUT_PIXEL_MASK(pos);
    else
        buf[OUT_PIXEL_IDX(pos)] &= ~OUT_PIXEL_MASK(pos);
}

static uint8_t* find_input_row(scale_stream_t *ctx, int row)
{
    for (int i = 0; i < SCALE_STREAM_MAX_ROWS; i++) {
        if (ctx->in_rows[i].row == row && ctx->in_rows[i].used) {
            return ctx->in_rows[i].buf;
        }
    }
    return NULL;
}

int scale_stream_process_out_row(scale_stream_t *ctx, size_t row, uint8_t *out_row_buf)
{
    int A, B, C, D, x, y, gray;
    float x_diff, y_diff;
    uint8_t *in_row[2];

    y = (int)(ctx->y_ratio * row);
    y_diff = (ctx->y_ratio * row) - y;

    for (int i = 0; i < 2; i++) {
        in_row[i] = find_input_row(ctx, y + i);
        if (!in_row[i]) {
            return -1;
        }
    }

    for (int j = 0; j < ctx->out_width; j++) {
        x = (int)(ctx->x_ratio * j);           
        x_diff = (ctx->x_ratio * j) - x;            

        A = get_pixel_row_in(in_row[0], x);
        B = get_pixel_row_in(in_row[0], x + 1);
        C = get_pixel_row_in(in_row[1], x);
        D = get_pixel_row_in(in_row[1], x + 1);


        // Y = A(1-w)(1-h) + B(w)(1-h) + C(h)(1-w) + Dwh
        gray = (int)(A * (1 - x_diff) * (1 - y_diff) + B * (x_diff) * (1 - y_diff) +
                     C * (y_diff) * (1 - x_diff) + D * (x_diff * y_diff));

        set_out_row_pixel(ctx, out_row_buf, j, (gray >= SCALE_STREAM_DEFAULT_GRAY_LEVEL));
    }

    return 0;
}

size_t scale_stream_check_row(scale_stream_t *ctx, size_t out_row)
{
    return (int)(ctx->y_ratio * out_row) + 1;
}

static void calculate_scale_preserve(scale_stream_t *ctx)
{
    float w_scale = (float)ctx->display_width / (float)ctx->in_width;
    float h_scale = (float)ctx->display_height / (float)ctx->in_height;

    if (w_scale <= h_scale)
    {
        ctx->out_width = ctx->display_width;
        ctx->out_height = ctx->in_height * w_scale;        
    }
    else
    {
        ctx->out_width = ctx->in_width * h_scale;
        ctx->out_height = ctx->display_height;
    }
}
