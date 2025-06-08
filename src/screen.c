#include "screen.h"
#include "io/io.h"
#include "drivers/keyboard/keyboard.h"

unsigned short g_cursor_pos = 0;
unsigned char cursor_prev_idx;

char* const g_fb = (char*)VIDEO_MEMORY_START;

#define CURSOR_MAX ((SCREEN_ROWS * SCREEN_COLUMNS) - 1)

static inline void clamp_cursor() {
    if (g_cursor_pos > CURSOR_MAX) g_cursor_pos = CURSOR_MAX;
    // g_cursor_pos is unsigned, so no need to check < 0
}

void fb_write_cell(unsigned short i, char c,unsigned char fg, unsigned char bg){
    g_fb[i * 2] = c;
    g_fb[i * 2 + 1] = ((bg & 0x0f) << 4 | (fg & 0x0f));
}

void clear_screen(){
    unsigned short i = 0;
    while (i <  SCREEN_ROWS * SCREEN_COLUMNS){
        fb_write_cell(i,' ',COLOR_BLACK,COLOR_BLACK);
        i++;
    }
    g_cursor_pos = 0;
}

void fb_set_cursor(unsigned short pos) {
    clamp_cursor();
    fb_write_cell(pos,' ',COLOR_LIGHT_GREY,COLOR_LIGHT_GREY);
}

void scroll_screen_up(){
    for (unsigned short idx = 0; idx < (SCREEN_ROWS * SCREEN_COLUMNS) - SCREEN_COLUMNS;idx++){
        g_fb[idx * 2] = g_fb[(idx + SCREEN_COLUMNS) * 2];
        g_fb[idx * 2 + 1] = g_fb[(idx + SCREEN_COLUMNS) * 2 + 1];
    }

    for(unsigned short idx = (SCREEN_ROWS * SCREEN_COLUMNS) - SCREEN_COLUMNS; idx <  (SCREEN_ROWS * SCREEN_COLUMNS);idx++){
        fb_write_cell(idx,' ',COLOR_BLACK,COLOR_BLACK);
    }
    if (g_cursor_pos >= SCREEN_COLUMNS)
        g_cursor_pos -= SCREEN_COLUMNS;
    else
        g_cursor_pos = 0;
    clamp_cursor();
    fb_set_cursor(g_cursor_pos);
}

void write_string(const char* str, const unsigned int len){
    for(unsigned int i = 0; i < len;i++){
        if (g_cursor_pos >= SCREEN_ROWS * SCREEN_COLUMNS){
            scroll_screen_up();
        }
        fb_write_cell(g_cursor_pos,str[i],COLOR_LIGHT_GREY,COLOR_BLACK);
        g_cursor_pos++;
        clamp_cursor();
    }
    fb_set_cursor(g_cursor_pos);
}

void newline(){
    if (g_cursor_pos < (SCREEN_COLUMNS -1) * SCREEN_ROWS){
        g_cursor_pos += (SCREEN_COLUMNS - (g_cursor_pos % SCREEN_COLUMNS));
        clamp_cursor();
    }
    fb_set_cursor(g_cursor_pos);
}

void handle_screen_input(){
    char c;
    if(kb_buffer_pop(&c)){
        switch (c)
        {
        case '\n':
            {
                fb_write_cell(g_cursor_pos,' ',COLOR_BLACK,COLOR_BLACK);//remove current cell
                newline();
            break;}
        case '\t':{
            if (g_cursor_pos < SCREEN_ROWS * SCREEN_COLUMNS - 4){
                g_cursor_pos += 4;
            } else {
                g_cursor_pos = CURSOR_MAX;
            }
            clamp_cursor();
            fb_set_cursor(g_cursor_pos);
            break;
        }
        case '\b':{
            fb_write_cell(g_cursor_pos,' ',COLOR_BLACK,COLOR_BLACK);//remove current cell
            if (g_cursor_pos != 0){
                g_cursor_pos--;
            }
            clamp_cursor();
            fb_write_cell(g_cursor_pos,' ',COLOR_BLACK,COLOR_BLACK);//remove current cell
            fb_set_cursor(g_cursor_pos);
            break;
        }
        default:{
                write_string(&c,1);
                break;
            }
        }
    }
    clamp_cursor();
    fb_set_cursor(g_cursor_pos);
}