
#ifndef INCLUDE_SCREEN_H
#define INCLUDE_SCREEN_H

#define VIDEO_MEMORY_START 0xc00b8000 // need to remap due to allocated memory pages

#define SCREEN_ROWS 25
#define SCREEN_COLUMNS 80

#define COLOR_BLACK 0x0
#define COLOR_BLUE 0x1
#define COLOR_GREEN 0x2
#define COLOR_CYAN 0x3
#define COLOR_RED 0x4
#define COLOR_MAGENTA 0x5
#define COLOR_BROWN 0x6
#define COLOR_LIGHT_GREY 0x7
#define COLOR_DARK_GREY 0x8
#define COLOR_LIGHT_BLUE 0x9
#define COLOR_LIGHT_GREEN 0xa
#define COLOR_LIGHT_CYAN 0xb
#define COLOR_LIGHT_RED 0xc
#define COLOR_LIGHT_MAGENTA 0xd
#define COLOR_LIGHT_BROWN 0xe
#define COLOR_WHITE 0xf

#define SCREEN_PIXELS (SCREEN_ROWS * SCREEN_COLUMNS)
#define CURSOR_MAX (SCREEN_PIXELS - 1)
#define INVALID_ARGUMENT -1
#include "filesystem/vfs/vfs.h"

extern unsigned short g_cursor_pos;
extern generic_file_t screen_file;
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
void fb_write_cell(unsigned short i, char c,unsigned char fg, unsigned char bg);

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
void fb_set_cursor(unsigned short pos);

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