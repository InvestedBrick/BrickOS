#include "cstdlib/stdutils.h"
#include "cstdlib/syscalls.h"
#include "cstdlib/stdio.h"
#include "../../src/filesystem/devices/device_defines.h"
#include "cstdlib/malloc.h"
typedef struct {
    uint32_t cmd;
    uint32_t pid;
    unsigned char data[];    
}win_man_msg_t;

typedef struct window{
    uint32_t origin_x;
    uint32_t origin_y;
    uint32_t width;
    uint32_t height;
    uint32_t flags;
    uint32_t owner_pid;
    int backing_fd;
    unsigned char* buffer;
    struct window* next;
}window_t;

window_t* head = 0; 

unsigned char* fb0 = 0;
uint32_t screen_bytes_per_row = 0;
uint32_t screen_bytespp = 0;
int wm_to_k_fd = 0;
int k_to_wm_fd = 0;

void add_window_to_list(window_t* new_win){
    window_t* curr = head;
    if (!curr) head = new_win;
    else {
        while(curr->next) curr = curr->next;
        curr->next = new_win;
    }
}

window_t* find_window_by_pid(uint32_t pid){
    window_t* curr = head;
    while(curr){
        if (curr->owner_pid == pid) return curr;
        curr = curr->next;
    }

    return 0;
}   

void handle_window_request(framebuffer_t* fb_metadata,win_man_msg_t* msg){
    window_req_t* req = (window_req_t*)msg->data;
    if (req->height > fb_metadata->height) req->height = fb_metadata->height;
    if (req->width > fb_metadata->width) req->width = fb_metadata->width;
    window_t* new_win = (window_t*)malloc(sizeof(window_t));
    
    new_win->flags = req->flags;
    new_win->width = req->width;
    new_win->height = req->height;
    new_win->owner_pid = msg->pid;
    new_win->next = 0;
    new_win->origin_x = (fb_metadata->width / 2) - 50;
    new_win->origin_y = (fb_metadata->height / 2) - 50;
    unsigned char* pid_str = uint32_to_ascii(new_win->owner_pid);
    uint32_t filename_len = sizeof("win_") + strlen(pid_str);
    unsigned char* backing_filename = (unsigned char*)malloc(filename_len);
    memcpy(backing_filename,"win_",sizeof("win_"));
    memcpy(&backing_filename[sizeof("win_") - 1],pid_str,strlen(pid_str));
    backing_filename[filename_len - 1] = '\0';
    free(pid_str);
    chdir("tmp"); // too lazy to parse directories when creating files rn
    new_win->backing_fd = open(backing_filename,FILE_FLAG_CREATE);
    chdir("..");
    
    new_win->buffer = (unsigned char*)mmap(new_win->width * new_win->height * sizeof(uint32_t),PROT_READ | PROT_WRITE,MAP_SHARED,new_win->backing_fd,0);
    // be sure that all pages are present, so that when the user process maps it, the file can be isntantly deleted (not good practise but eh, probably only temporary solution)
    memset(new_win->buffer,0,new_win->width * new_win->height * sizeof(uint32_t));
    close(new_win->backing_fd);
    
    window_creation_ans_t creation_ans =
    {.width = new_win->width,
            .height = new_win->height, 
            .filename_len = filename_len,
            .pid = new_win->owner_pid,
            .answer_type = DEV_WM_ANS_TYPE_WIN_CREATION,
        };
    // always pass the answer type first
    write(wm_to_k_fd,(unsigned char*)&creation_ans,sizeof(window_creation_ans_t)); 
    write(wm_to_k_fd,backing_filename,filename_len);
    
    add_window_to_list(new_win);
    free(backing_filename);
}

void handle_window_commit(framebuffer_t* fb_metadata,win_man_msg_t* msg){
    window_t* win = find_window_by_pid(msg->pid);
    if (!win) return;
    unsigned char* win_start = (fb0 + win->origin_y * fb_metadata->bytes_per_row + win->origin_x * screen_bytespp);
    // cap so that we dont copy beyond framebuffer boundaries
    uint32_t width_copy_size = ((win->origin_x + win->width > fb_metadata->width) ? (fb_metadata->width - win->origin_x) : win->width) * screen_bytespp;
    uint32_t rows_to_copy = (win->origin_y + win->height > fb_metadata->height) ? (fb_metadata->height - win->origin_y) : win->height;
    
    uint32_t buffer_cpy_off = 0;
    for(uint32_t i = 0; i < rows_to_copy;i++){
        // no questions asked here
        memcpy(win_start,&win->buffer[buffer_cpy_off],width_copy_size);
        win_start += fb_metadata->bytes_per_row;
    }
}

__attribute__((section(".text.start")))
void main(){
    int fb0_fd = open("dev/fb0",FILE_FLAG_NONE);
    wm_to_k_fd = open("tmp/wm_to_k.tmp",FILE_FLAG_WRITE);
    k_to_wm_fd = open("tmp/k_to_wm.tmp",FILE_FLAG_READ);

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
    
    while(1){
        memset(buffer,0,sizeof(buffer));        
        while(read(k_to_wm_fd,buffer,sizeof(buffer)) <= 0){}
        win_man_msg_t* msg = (win_man_msg_t*)buffer;
        switch (msg->cmd)
        {
        case DEV_WM_REQUEST_WINDOW:{
            handle_window_request(&fb_metadata,msg);
            break;
        }
        case DEV_WM_COMMIT_WINDOW: {
            handle_window_commit(&fb_metadata,msg);
            break;
        }

    
        }
    }

    exit(EXIT_SUCCESS);
}
