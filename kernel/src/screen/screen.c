#include "screen.h"
#include "../io/io.h"
#include "../io/log.h"
#include "../filesystem/vfs/vfs.h"
#include "../filesystem/devices/devs.h"
#include "../utilities/util.h"
#include "../memory/memory.h"
#include "font_bitmaps.h"
#include <stdint.h>
#include <stddef.h>
#include "../../limine-protocol/include/limine.h"
#include "../kernel_header.h"
volatile unsigned char* fb = 0;

void init_framebuffer(){
    uint64_t fb_phys = (uint64_t)limine_data.framebuffer->address - limine_data.hhdm;
    uint32_t fb_size = limine_data.framebuffer->pitch * limine_data.framebuffer->height;

    //compacted so that I can ship it compactly to the window manager
    fb0.bpp            = limine_data.framebuffer->bpp;
    fb0.bytes_per_row  = limine_data.framebuffer->pitch;
    fb0.height         = limine_data.framebuffer->height;
    fb0.width          = limine_data.framebuffer->width;
    fb0.phys_addr      = fb_phys;
    fb0.size           = fb_size;

}
