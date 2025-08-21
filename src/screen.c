#include "screen.h"
#include "io/io.h"
#include "drivers/keyboard/keyboard.h"
#include "filesystem/vfs/vfs.h"
#include "util.h"
#include "memory/memory.h"
#include "multiboot.h"

char* const g_fb = (char*)VIDEO_MEMORY_START;
unsigned short g_cursor_pos = 0;
unsigned char cursor_prev_idx;

volatile unsigned char* fb = 0;
unsigned int screen_width;
unsigned int screen_height;
unsigned int screen_bytes_per_row;
unsigned char screen_bytespp;

// preparation for multiple screens
unsigned int screen_origin_x = 0;
unsigned int screen_origin_y = 0;
void map_framebuffer(unsigned int fb_phys,unsigned int fb_virt, unsigned int fb_size){
    unsigned int n_pages = CEIL_DIV(fb_size,MEMORY_PAGE_SIZE);
    for (unsigned int i = 0; i < n_pages;i++){
        mem_map_page(fb_virt + i * MEMORY_PAGE_SIZE, fb_phys + i * MEMORY_PAGE_SIZE, PAGE_FLAG_WRITE | PAGE_FLAG_OWNER);
    } 
}

void init_framebuffer(multiboot_info_t* mboot,unsigned int fb_start){

    unsigned int fb_phys = (unsigned int)mboot->framebuffer_addr;
    unsigned int fb_size = mboot->framebuffer_pitch * mboot->framebuffer_height;

    fb = (volatile unsigned char*)(fb_start);
    screen_width             = mboot->framebuffer_width;
    screen_height            = mboot->framebuffer_height;
    screen_bytes_per_row     = mboot->framebuffer_pitch;
    screen_bytespp           = mboot->framebuffer_bpp / 8;
    map_framebuffer(fb_phys,fb_start,fb_size);
}

void write_pixel(unsigned int x, unsigned int y, unsigned int color){
    if (x >= screen_width || x < screen_origin_x) return;
    if (y >= screen_height || y < screen_origin_y) return;

    *(volatile unsigned int*)(fb + y * screen_bytes_per_row + x * screen_bytespp) = color;
}

void clamp_cursor() {
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

void erase_one_char(){
    fb_write_cell(g_cursor_pos,' ',COLOR_BLACK,COLOR_BLACK);//remove current cell
    if (g_cursor_pos != 0){
        g_cursor_pos--;
    }
    clamp_cursor();
    fb_write_cell(g_cursor_pos,' ',COLOR_BLACK,COLOR_BLACK);//remove current cell
    fb_set_cursor(g_cursor_pos);
}

void newline(){
    fb_write_cell(g_cursor_pos,' ',COLOR_BLACK,COLOR_BLACK);//remove current cell
    if (g_cursor_pos < (SCREEN_COLUMNS - 1) * SCREEN_ROWS){
        g_cursor_pos += (SCREEN_COLUMNS - (g_cursor_pos % SCREEN_COLUMNS));
        clamp_cursor();
    }
    fb_set_cursor(g_cursor_pos);
}

void write_char(unsigned char c){
    if (g_cursor_pos >= SCREEN_ROWS * SCREEN_COLUMNS){scroll_screen_up();}
    fb_write_cell(g_cursor_pos,c,COLOR_LIGHT_GREY,COLOR_BLACK);
    g_cursor_pos++;
    clamp_cursor();
    fb_set_cursor(g_cursor_pos);
}

void handle_screen_char_input(unsigned char c){
    switch (c)
        {
        case '\n':{
            newline();
            break;
        }
        case '\e': { // just an esc char that gcc does not complain about
            clear_screen();
            break;
        }
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
            erase_one_char();
            break;
        }
        default:{
            write_char(c);
            break;
        }
    }
    
    clamp_cursor();
    fb_set_cursor(g_cursor_pos);
}

int screen_read(generic_file_t* f, unsigned char* buffer, unsigned int size){
    if (size > SCREEN_PIXELS)
        return INVALID_ARGUMENT;

    for (unsigned int i = 0; i < size; i++){
        buffer[i] = g_fb[i * 2];
    }

    return size;
}

int screen_write(generic_file_t* f, unsigned char* buffer,unsigned int size){
    if (size > SCREEN_PIXELS) 
        return INVALID_ARGUMENT;
    
    for(unsigned int i = 0; i < size;i++){
        handle_screen_char_input(buffer[i]);
    }

    return size;
        
}

vfs_handlers_t screen_ops = {

    .close = 0,
    .open = 0,
    .read = screen_read,
    .write = screen_write,
};

generic_file_t screen_file = {
    .ops = &screen_ops,
    .generic_data = 0
};


void disable_cursor(){
	outb(FB_COMMAND_PORT, FB_DISABLE_CURSOR_COMMAND);
	outb(FB_DATA_PORT, FB_DISABLE_CURSOR_DATA);
}
