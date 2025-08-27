#include "cstdlib/stdutils.h"
#include "cstdlib/syscalls.h"
#include "cstdlib/stdio.h"
#include "../../src/filesystem/devices/device_defines.h"
unsigned char* fb0 = 0;
uint32_t screen_bytes_per_row = 0;
uint32_t screen_bytespp = 0;

void write_pixel(unsigned char* fb,uint32_t x, uint32_t y, uint32_t color){

    *(volatile uint32_t*)(fb + y * screen_bytes_per_row + x * screen_bytespp) = color;
}

__attribute__((section(".text.start")))
void main(){
    int fb0_fd = open("dev/fb0",FILE_FLAG_NONE);
    int wm_to_k_fd = open("tmp/wm_to_k.tmp",FILE_FLAG_WRITE);
    int k_to_wm_fd = open("tmp/k_to_wm.tmp",FILE_FLAG_READ);

    if (fb0_fd < 0) exit(1);
    if (wm_to_k_fd < 0) exit(2);
    if (k_to_wm_fd < 0) exit(3);
    framebuffer_t fb_metadata;
    if (ioctl(fb0_fd,DEV_FB0_GET_METADATA,&fb_metadata) < 0) exit(4);
    
    screen_bytes_per_row = fb_metadata.bytes_per_row;
    screen_bytespp = fb_metadata.bpp / 8;

    fb0 = (unsigned char*)mmap(fb_metadata.size, PROT_READ | PROT_WRITE, MAP_SHARED, fb0_fd,0);
    
    close(fb0_fd);
    if (!fb0) exit(5);


    for (uint32_t i = 200; i < 230;i++){
        for (uint32_t j = 300; j < 330;j++){
            write_pixel(fb0,j,i,0xff00ff00);
        }
    }

    unsigned char buffer[128] = {0};

    if (read(k_to_wm_fd,buffer,128) < 0) exit(6);

    print(buffer);

    exit(EXIT_SUCCESS);
}
