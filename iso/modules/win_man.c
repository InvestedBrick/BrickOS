#include "cstdlib/stdutils.h"
#include "cstdlib/syscalls.h"
#include "cstdlib/stdio.h"
#include "../../src/filesystem/devices/device_defines.h"
#include "cstdlib/malloc.h"
typedef struct {
    uint32_t cmd;
    uint32_t pid;
    void* data;    
}win_man_msg_t;

typedef struct window{
    uint32_t width;
    uint32_t height;
    uint32_t flags;
    uint32_t owner_pid;
    uint32_t backing_fd;;
    struct window* next;
}window_t;

window_t* head = 0; 

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
    
    unsigned char buffer[128] = {0};
    memset(buffer,0,sizeof(buffer));
    if (read(k_to_wm_fd,buffer,sizeof(buffer)) < 0) exit(6);

    print(buffer);

    while(read(k_to_wm_fd,buffer,sizeof(buffer)) < 0){};
    memset(buffer,0,sizeof(buffer));
    uint32_t cmd = *(uint32_t*)&buffer[0];

    switch (cmd)
    {
    case DEV_WM_REQUEST_WINDOW:{
        win_man_msg_t* msg = (win_man_msg_t*)buffer;
        window_req_t* req = (window_req_t*)msg->data;

        if (req->height > fb_metadata.height) break;
        if (req->width > fb_metadata.width) break;

        window_t* new_win = (window_t*)malloc(sizeof(window_t));
        
        new_win->flags = req->flags;
        new_win->width = req->width;
        new_win->height = req->height;
        new_win->owner_pid = msg->pid;
        new_win->next = 0;
        unsigned char* pid_str = uint32_to_ascii(new_win->owner_pid);
        uint32_t filename_len = sizeof("win_") + strlen(pid_str);
        unsigned char* backing_filename = (unsigned char*)malloc(filename_len);
        memcpy(&backing_filename[sizeof("win_") - 1],pid_str,strlen(pid_str));
        free(pid_str);

        chdir("tmp"); // too layzy to parse directories when creating files rn
        new_win->backing_fd = open(backing_filename,FILE_FLAG_CREATE);
        chdir("..");
        //TODO: map memory and return to user)
        window_t* curr = head;
        if (!curr) head = new_win;
        else {
            while(curr->next) curr = curr->next;
            curr->next = new_win;
        }

        break;
    }
    
    }

    exit(EXIT_SUCCESS);
}
