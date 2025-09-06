#include "screen.h"
#include "../io/io.h"
#include "../io/log.h"
#include "../drivers/keyboard/keyboard.h"
#include "../filesystem/vfs/vfs.h"
#include "../filesystem/devices/devs.h"
#include "../util.h"
#include "../memory/memory.h"
#include "../multiboot.h"
#include "font_bitmaps.h"
#include <stdint.h>

volatile unsigned char* fb = 0;

uint8_t screen_bytespp;

uint8_t r_pos = 0;
uint8_t r_mask_size = 0;

uint8_t g_pos = 0;
uint8_t g_mask_size = 0;

uint8_t b_pos = 0;
uint8_t b_mask_size = 0;

// preparation for multiple screens
uint32_t gfx_cursor_x = 0;
uint32_t gfx_cursor_y = 0;
uint32_t screen_cursor_x_max = 0;
uint32_t screen_cursor_y_max = 0;

uint32_t screen_bytes_per_row;
uint32_t screen_origin_x = 0;
uint32_t screen_origin_y = 0;
uint32_t screen_width;
uint32_t screen_height;
uint32_t rgb_to_color(uint8_t r, uint8_t g, uint8_t b) {
    // assuming 32bit ARGB
    uint32_t color = 0;

    color |= ((uint32_t)r << 16);
    color |= ((uint32_t)g << 8);
    color |= ((uint32_t)b << 0);

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

    fb = (volatile unsigned char*)(fb_start);
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

    screen_cursor_x_max = screen_width / COLUMNS_PER_CHAR;
    screen_cursor_y_max = screen_height / ROWS_PER_CHAR;

    fb0.bpp            = mboot->framebuffer_bpp;
    fb0.bytes_per_row  = mboot->framebuffer_pitch;
    fb0.height         = mboot->framebuffer_height;
    fb0.width          = mboot->framebuffer_width;
    fb0.phys_addr      = fb_phys;
    fb0.size           = fb_size;

    map_framebuffer(fb_phys,fb_start,fb_size);
}
void clamp_cursor() {
    if (gfx_cursor_x > screen_cursor_x_max) {gfx_cursor_x = 0;gfx_cursor_y++;}
    if (gfx_cursor_y > screen_cursor_y_max) gfx_cursor_y = screen_cursor_y_max;
}

/* Removed framebuffer writing functions
void write_pixel(uint32_t x, uint32_t y, uint32_t color){
    if (x >= screen_width || x < screen_origin_x) return;
    if (y >= screen_height || y < screen_origin_y) return;

    *(volatile uint32_t*)(fb + y * screen_bytes_per_row + x * screen_bytespp) = color;
}

void write_char(uint8_t ch,uint32_t fg, uint32_t bg){
    if (ch < ' ' || ch > '~') return;
    const uint8_t map_idx = ch - ' ';

    uint32_t px = gfx_cursor_x * COLUMNS_PER_CHAR;
    uint32_t py = gfx_cursor_y * ROWS_PER_CHAR; // work up from the bottom

    for (uint32_t i = 0; i < ROWS_PER_CHAR;i++){
        uint8_t row = char_bitmap_8x16[map_idx][i];

        for (uint8_t bit_idx = 0; bit_idx < 8; bit_idx++) {
             
            write_pixel(px + (7 - bit_idx), py, row & (1 << bit_idx) ? fg : bg);
        }

        py++; 

    }
    gfx_cursor_x++;
    clamp_cursor();
}

void print_bitmap(){
    for (uint8_t ch = ' '; ch <= '~';ch++){
        write_char(ch,VBE_COLOR_GRAY,VBE_COLOR_BLACK);
    }
}

void clear_screen(uint32_t color){
    for (uint32_t i = screen_origin_y; i < screen_origin_y + screen_height;i++ ){
        for (uint32_t j = screen_origin_x; j < screen_origin_x + screen_width;j++){
            write_pixel(j,i,color);
        }
    }

    gfx_cursor_x = 0;
    gfx_cursor_y = 0;
}

void move_cursor_one_back(){
    if (gfx_cursor_x != 0){
        gfx_cursor_x--;
    }else{
        if (gfx_cursor_y != 0){
            gfx_cursor_x = screen_cursor_x_max;
            gfx_cursor_y = 0;
        }
    }
}

void update_cursor() {
    write_char(' ',VBE_COLOR_GRAY,VBE_COLOR_GRAY);
    move_cursor_one_back();
    clamp_cursor();
}

void erase_one_char(){
    write_char(' ',VGA_COLOR_BLACK,VGA_COLOR_BLACK);
    move_cursor_one_back();
    move_cursor_one_back();
    update_cursor();
}

void newline(){

    write_char(' ',VBE_COLOR_BLACK,VBE_COLOR_BLACK);
    if (gfx_cursor_y < screen_cursor_y_max - 1){
        gfx_cursor_y++;
        gfx_cursor_x = 0;
    }
    update_cursor();
}

void handle_screen_char_input(uint8_t c){
    switch (c)
        {
        case '\n':{
            newline();
            break;
        }
        case '\e': { // just an esc char that gcc does not complain about
            clear_screen(VBE_COLOR_BLACK);
            break;
        }
        case '\t':{
            gfx_cursor_x += 4;
            break;
        }
        case '\b':{
            erase_one_char();
            break;
        }
        default:{
            write_char(c,VBE_COLOR_GRAY,VBE_COLOR_BLACK);
            break;
        }
    }
    
    clamp_cursor();
    update_cursor();
}
*/

int screen_write(generic_file_t* f, unsigned char* buffer,uint32_t size){
    if (size > screen_width * screen_height) 
        return INVALID_ARGUMENT;
    

    return size;
        
}

vfs_handles_t screen_ops = {

    .close = 0,
    .open = 0,
    .read = 0,
    .write = screen_write,
    .seek = 0,
    .ioctl = 0,
};

generic_file_t screen_file = {
    .ops = &screen_ops,
    .generic_data = 0
};

