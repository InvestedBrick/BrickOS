#include "screen.h"
#include "io/io.h"
#include "io/log.h"
#include "drivers/keyboard/keyboard.h"
#include "filesystem/vfs/vfs.h"
#include "util.h"
#include "memory/memory.h"
#include "multiboot.h"
#include <stdint.h>
char* g_fb = (char*)VIDEO_MEMORY_START;
uint16_t g_cursor_pos = 0;
uint8_t cursor_prev_idx;

volatile uint8_t* fb = 0;
uint32_t screen_width;
uint32_t screen_height;
uint32_t screen_bytes_per_row;
uint8_t screen_bytespp;

uint8_t r_pos = 0;
uint8_t r_mask_size = 0;

uint8_t g_pos = 0;
uint8_t g_mask_size = 0;

uint8_t b_pos = 0;
uint8_t b_mask_size = 0;

// preparation for multiple screens
uint32_t screen_origin_x = 0;
uint32_t screen_origin_y = 0;

uint32_t rgb_to_color(uint8_t r, uint8_t g, uint8_t b) {
    // assuming 32bit ARGB
    uint32_t color = 0;

    color |= ((uint32_t)r << 16);
    color |= ((uint32_t)g << 8);
    color |= ((uint32_t)b << 0);

    // optional: force alpha to opaque if your format uses it
    color |= 0xff000000;

    return color;
}

void map_framebuffer(uint32_t fb_phys,uint32_t fb_virt, uint32_t fb_size){

    uint32_t n_pages = CEIL_DIV(fb_size,MEMORY_PAGE_SIZE);
    for (uint32_t i = 0; i < n_pages;i++){
        mem_map_page(fb_virt + i * MEMORY_PAGE_SIZE,fb_phys + i * MEMORY_PAGE_SIZE, PAGE_FLAG_WRITE | PAGE_FLAG_OWNER);
    } 
}

void init_framebuffer(multiboot_info_t* mboot,uint32_t fb_start){

    uint32_t fb_phys = (uint32_t)mboot->framebuffer_addr;
    uint32_t fb_size = mboot->framebuffer_pitch * mboot->framebuffer_height;

    fb = (volatile uint8_t*)(fb_start);
    screen_width             = mboot->framebuffer_width;
    screen_height            = mboot->framebuffer_height;
    screen_bytes_per_row     = mboot->framebuffer_pitch;
    screen_bytespp           = mboot->framebuffer_bpp / 8;
    r_pos                    = mboot->framebuffer_red_field_pos;
    r_mask_size              = mboot->framebuffer_red_mask_size;
    g_pos                    = mboot->framebuffer_green_field_pos;
    g_mask_size              = mboot->framebuffer_green_mask_size;
    b_pos                    = mboot->framebuffer_blue_field_pos;
    b_mask_size              = mboot->framebuffer_blue_mask_size;

    map_framebuffer(fb_phys,fb_start,fb_size);
}

void write_pixel(uint32_t x, uint32_t y, uint32_t color){
    if (x >= screen_width || x < screen_origin_x) return;
    if (y >= screen_height || y < screen_origin_y) return;

    *(volatile uint32_t*)(fb + y * screen_bytes_per_row + x * screen_bytespp) = color;
}

void clamp_cursor() {
    if (g_cursor_pos > CURSOR_MAX) g_cursor_pos = CURSOR_MAX;
    // g_cursor_pos is unsigned, so no need to check < 0
}

void fb_write_cell(uint16_t i, char c,uint8_t fg, uint8_t bg){
    g_fb[i * 2] = c;
    g_fb[i * 2 + 1] = ((bg & 0x0f) << 4 | (fg & 0x0f));
}

void clear_screen(){
    uint16_t i = 0;
    while (i <  SCREEN_ROWS * SCREEN_COLUMNS){
        fb_write_cell(i,' ',COLOR_BLACK,COLOR_BLACK);
        i++;
    }
    g_cursor_pos = 0;
}

void fb_set_cursor(uint16_t pos) {
    clamp_cursor();
    fb_write_cell(pos,' ',COLOR_LIGHT_GREY,COLOR_LIGHT_GREY);
}

void scroll_screen_up(){
    for (uint16_t idx = 0; idx < (SCREEN_ROWS * SCREEN_COLUMNS) - SCREEN_COLUMNS;idx++){
        g_fb[idx * 2] = g_fb[(idx + SCREEN_COLUMNS) * 2];
        g_fb[idx * 2 + 1] = g_fb[(idx + SCREEN_COLUMNS) * 2 + 1];
    }

    for(uint16_t idx = (SCREEN_ROWS * SCREEN_COLUMNS) - SCREEN_COLUMNS; idx <  (SCREEN_ROWS * SCREEN_COLUMNS);idx++){
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

void write_char(uint8_t c){
    if (g_cursor_pos >= SCREEN_ROWS * SCREEN_COLUMNS){scroll_screen_up();}
    fb_write_cell(g_cursor_pos,c,COLOR_LIGHT_GREY,COLOR_BLACK);
    g_cursor_pos++;
    clamp_cursor();
    fb_set_cursor(g_cursor_pos);
}

void handle_screen_char_input(uint8_t c){
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

int screen_read(generic_file_t* f, uint8_t* buffer, uint32_t size){
    if (size > SCREEN_PIXELS)
        return INVALID_ARGUMENT;

    for (uint32_t i = 0; i < size; i++){
        buffer[i] = g_fb[i * 2];
    }

    return size;
}

int screen_write(generic_file_t* f, uint8_t* buffer,uint32_t size){
    if (size > SCREEN_PIXELS) 
        return INVALID_ARGUMENT;
    
    for(uint32_t i = 0; i < size;i++){
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
