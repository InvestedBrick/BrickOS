#include "cstdlib/stdutils.h"
#include "cstdlib/syscalls.h"
#include "cstdlib/stdio.h"
#include "../../src/filesystem/devices/device_defines.h"
unsigned char* fb = 0;
uint32_t screen_bytes_per_row = 0;
uint32_t screen_bytespp = 0;

void write_pixel(uint32_t x, uint32_t y, uint32_t color){

    *(volatile uint32_t*)(fb + y * screen_bytes_per_row + x * screen_bytespp) = color;
}

__attribute__((section(".text.start")))
void main(){
    int fb0_fd = open("dev/fb0",FILE_FLAG_NONE);

    if (fb0_fd < 0) exit(EXIT_FAILURE);
    
    framebuffer_t fb_metadata;
    if (ioctl(fb0_fd,DEV_FB0_GET_METADATA,&fb_metadata) < 0) exit(EXIT_FAILURE * 2);
    

    screen_bytes_per_row = fb_metadata.bytes_per_row;
    screen_bytespp = fb_metadata.bpp / 8;

    fb = (unsigned char*)mmap(fb_metadata.size, PROT_READ | PROT_WRITE, MAP_SHARED, fb0_fd,0);
    
    if (!fb) exit(EXIT_FAILURE * 3);

    for (uint32_t i = 300; i < 330;i++){
        for (uint32_t j = 300; j < 330;j++){
            write_pixel(j,i,0xff00ff00);
        }
    }

    exit(EXIT_SUCCESS);
}
