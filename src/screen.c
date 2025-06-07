#include "screen.h"
#include "io/io.h"

unsigned short g_cursor_pos = 0;

char* const fb = (char*)VIDEO_MEMORY_START;

void fb_write_cell(unsigned short i, char c,unsigned char fg, unsigned char bg){
    fb[i * 2] = c;
    fb[i * 2 + 1] = ((bg & 0x0f) << 4 | (fg & 0x0f));
}

void clear_screen(){
    unsigned short i = 0;
    while (i <  SCREEN_ROWS * SCREEN_COLUMNS){
        fb_write_cell(i,' ',COLOR_BLACK,COLOR_BLACK);
        i++;
    }
}

void fb_set_cursor(unsigned short pos) {
    outb(FB_COMMAND_PORT,FB_HIGH_BYTE_COMMAND);
    outb(FB_DATA_PORT, ((pos>>8) & 0x00ff));
    outb(FB_COMMAND_PORT, FB_LOW_BYTE_COMMAND);
    outb(FB_DATA_PORT, pos & 0x00ff);
}

void scroll_screen_up(){
    for (unsigned short idx = 0; idx < (SCREEN_ROWS * SCREEN_COLUMNS) - SCREEN_COLUMNS;idx++){
        fb[idx * 2] = fb[idx * 2 + SCREEN_COLUMNS];
        fb[idx * 2 + 1] = fb[idx * 2 + SCREEN_COLUMNS  + 1];
    }

    for(unsigned short idx = (SCREEN_ROWS * SCREEN_COLUMNS) - SCREEN_COLUMNS; idx <  (SCREEN_ROWS * SCREEN_COLUMNS);idx++){
        fb_write_cell(idx,' ',COLOR_BLACK,COLOR_BLACK);
    }
    g_cursor_pos -= SCREEN_COLUMNS;
}

void write_string(const char* str, const unsigned int len){
    for(unsigned int i = 0; i < len;i++){
        if (g_cursor_pos == (SCREEN_ROWS * SCREEN_COLUMNS)){
            scroll_screen_up();
        }
        fb_write_cell(g_cursor_pos,str[i],COLOR_LIGHT_GREY,COLOR_BLACK);
        g_cursor_pos++;
        fb_set_cursor(g_cursor_pos);
    }
}