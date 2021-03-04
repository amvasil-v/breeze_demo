// Header (.h) for font: b'Code' b'Regular' 16

#include <stdint.h>

extern const int TALLEST_CHAR_PIXELS;

extern const uint8_t b_code_b_regular_16_font_pixels[];

struct font_char {
    int offset;
    int w;
    int h;
    int left;
    int top;
    int advance;
};

extern const struct font_char b_code_b_regular_16_font_lookup[];
