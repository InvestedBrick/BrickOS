#ifndef INCLUDE_WINDOW_H
#define INCLUDE_WINDOW_H

#include <stdint.h>
#include "syscalls.h"
#include "../../../kernel/src/filesystem/devices/device_defines.h"
#include "font_bitmaps.h"
#include "malloc.h"
#include "stdutils.h"

typedef struct{
    unsigned char* fb;
    uint32_t win_height;
    uint32_t win_width;
    uint32_t cursor_x;
    uint32_t cursor_y;
    uint32_t cursor_x_max;
    uint32_t cursor_y_max;
    int kb_fd;
} user_fb_t;

void request_window(user_fb_t* fb, uint32_t width, uint32_t height);
void commit_window();
void delete_window();
void write_pixel(user_fb_t* fb, uint32_t x, uint32_t y, uint32_t color);
void write_char(user_fb_t* fb, uint8_t ch, uint32_t fg, uint32_t bg);

#endif