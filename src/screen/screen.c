#include "screen.h"
#include "../io/io.h"
#include "../io/log.h"
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
