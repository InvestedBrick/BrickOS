
#ifndef INCLUDE_SCREEN_H
#define INCLUDE_SCREEN_H

#define VIDEO_MEMORY_START 0xc00b8000 // need to remap due to allocated memory pages
#define SCREEN_PIXEL_BUFFER_START 0xe0000000

#define SCREEN_ROWS 25
#define SCREEN_COLUMNS 80

#define VGA_COLOR_BLACK 0x0
#define VGA_COLOR_BLUE 0x1
#define VGA_COLOR_GREEN 0x2
#define VGA_COLOR_CYAN 0x3
#define VGA_COLOR_RED 0x4
#define VGA_COLOR_MAGENTA 0x5
#define VGA_COLOR_BROWN 0x6
#define VGA_COLOR_LIGHT_GREY 0x7
#define VGA_COLOR_DARK_GREY 0x8
#define VGA_COLOR_LIGHT_BLUE 0x9
#define VGA_COLOR_LIGHT_GREEN 0xa
#define VGA_COLOR_LIGHT_CYAN 0xb
#define VGA_COLOR_LIGHT_RED 0xc
#define VGA_COLOR_LIGHT_MAGENTA 0xd
#define VGA_COLOR_LIGHT_BROWN 0xe
#define VGA_COLOR_WHITE 0xf


#define VBE_COLOR_BLACK 0x00000000
#define VBE_COLOR_WHITE 0xffffffff
#define VBE_COLOR_GRAY 0xffcbccca
#define VBE_COLOR_RED 0xffff0000

#define SCREEN_PIXELS (SCREEN_ROWS * SCREEN_COLUMNS)
#define CURSOR_MAX (SCREEN_PIXELS - 1)
#define INVALID_ARGUMENT -1
#include "../filesystem/vfs/vfs.h"
#include "../multiboot.h"
#include <stdint.h>
extern uint16_t g_cursor_pos;
extern generic_file_t screen_file;

void print_bitmap();

uint32_t rgb_to_color(uint8_t r, uint8_t g, uint8_t b);
/**
 * init_framebuffer:
 * Initializes the framebuffer
 * @param mboot The multiboot information
 * @param fb_start The start of the buffer in virtual memory
 */
void init_framebuffer(multiboot_info_t* mboot,uint32_t fb_start);

/**
 * write_pixel:
 * Writes a pixel with a given color to the screen
 * 
 * @param x The x coordinate of the pixel
 * @param y The y coordinate of the pixel
 * @param color The color of the pixel (ARGB format)
 */
void write_pixel(uint32_t x, uint32_t y, uint32_t color);

/**
*  fb_write_cell:
*  writes a char with given foreground and background color to position i in the framebuffer
* 
* @param i The location in the framebuffer
* @param c The char
* @param fg The foreground color
* @param bg The background color
*  
* */ 
void fb_write_cell(uint16_t i, char c,uint8_t fg, uint8_t bg);

/**
* clear_screen:
* Clears the screen of the OS terminal
*
*/
void clear_screen();

/**
 * fb_move_cursor:
 * moves the cursor to a given position
 * 
 * @param pos The new position of the cursor
 */
void fb_set_cursor(uint16_t pos);

/**
 * scroll_screen_up:
 * scrolls the screen up by one row
 */
void scroll_screen_up();

/**
 * newline: 
 * Prints a newline
 */
void newline();
/**
 * erase_one_char: 
 * Erases one character from the screen
 */
void erase_one_char();

/**
 * disable_cursor: 
 * disables the standard VGA cursor
 */
void disable_cursor();
#endif