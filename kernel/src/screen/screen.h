
#ifndef INCLUDE_SCREEN_H
#define INCLUDE_SCREEN_H

#include "../memory/memory.h"
#define SCREEN_PIXEL_BUFFER_START (HHDM) + 0x100000000

#define INVALID_ARGUMENT -1
#include "../filesystem/vfs/vfs.h"
#include <stdint.h>

/**
 * init_framebuffer:
 * Initializes the framebuffer
 */
void init_framebuffer();

#endif