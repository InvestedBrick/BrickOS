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
#include <stddef.h>
#include "../../../limine/limine.h"
#include "../kernel_header.h"
volatile unsigned char* fb = 0;

void map_framebuffer(uint64_t fb_phys,uint64_t fb_virt, uint32_t fb_size){

    uint32_t n_pages = CEIL_DIV(fb_size,MEMORY_PAGE_SIZE);
    for (uint32_t i = 0; i < n_pages;i++){
        mem_map_page(fb_virt + i * MEMORY_PAGE_SIZE,fb_phys + i * MEMORY_PAGE_SIZE, PAGE_FLAG_WRITE);
    } 
}

void init_framebuffer(uint64_t fb_start){

    uint64_t fb_phys = (uint64_t)limine_data.framebuffer - limine_data.hhdm;
    uint32_t fb_size = limine_data.framebuffer->pitch * limine_data.framebuffer->height;

    fb = (volatile unsigned char*)(fb_start);

    //compacted so that I can ship it compactly to the window manager
    fb0.bpp            = limine_data.framebuffer->bpp;
    fb0.bytes_per_row  = limine_data.framebuffer->pitch;
    fb0.height         = limine_data.framebuffer->height;
    fb0.width          = limine_data.framebuffer->width;
    fb0.phys_addr      = fb_phys;
    fb0.size           = fb_size;

    map_framebuffer(fb_phys,fb_start,fb_size);
}
