#ifndef INCLUDE_BITMAP_H
#define INCLUDE_BITMAP_H
#include <stdint.h>

#define N_CHARS 95
#define CHAR_WIDTH 8
#define CHAR_HEIGHT 16
#define COLUMNS_PER_CHAR (CHAR_WIDTH + 2)  // +2 for spacing
#define ROWS_PER_CHAR CHAR_HEIGHT
// an 8x13 pixel character bitmap (+2 for empty space)
// this one is upside down (need to start py at gfx_cursor_y * ROWS_PER_CHAR + ROWS_PER_CHAR and count down)
extern const uint8_t char_bitmap_8x15[N_CHARS][ROWS_PER_CHAR];

extern const uint8_t char_bitmap_8x16[N_CHARS][16];
#endif