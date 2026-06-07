#include "window.h"

void request_window(user_fb_t* user_fb,uint32_t width,uint32_t height){
    int wm_fd = open("dev/wm",FILE_FLAG_NONE);
    chdir("tmp"); 
    
    window_req_t win_req;
    win_req.flags = 0;
    win_req.height = height;
    win_req.width = width;
    ioctl(wm_fd,DEV_WM_REQUEST_WINDOW,&win_req);



    window_creation_wm_answer_t* answer = (window_creation_wm_answer_t*)malloc(sizeof(window_creation_wm_answer_t) + 256); // 256 for filename
    memset(answer,0x0,sizeof(window_creation_wm_answer_t) + 256);
    
    while (ioctl(wm_fd,DEV_WM_REQUEST_WINDOW_CREATION_ANSWER,answer) < 0){}
    unsigned char* pid_str = uint32_to_ascii(getpid());
    chdir("wm");
    chdir(pid_str);

    int backing_fd = open(answer->filename,FILE_FLAG_NONE);
    user_fb->kb_fd = answer->kb_fd;
    user_fb->fb = (unsigned char*)mmap(answer->width * answer->height * sizeof(uint32_t),PROT_READ | PROT_WRITE, MAP_SHARED,backing_fd,0);
    
    user_fb->win_height = answer->height;
    user_fb->win_width = answer->width;
    user_fb->cursor_x_max = user_fb->win_width / COLUMNS_PER_CHAR;
    user_fb->cursor_y_max = user_fb->win_height / ROWS_PER_CHAR;
    user_fb->cursor_x = 0;
    user_fb->cursor_y = 0;

    close(backing_fd);
    rmfile(answer->filename); // dispose of the connector
    chdir("../../..");
    free(pid_str);
    free(answer);
    close(wm_fd);
    
}

void commit_window(){
    int wm_fd = open("dev/wm",FILE_FLAG_NONE);
    ioctl(wm_fd,DEV_WM_COMMIT_WINDOW,0);
    close(wm_fd);
}

void delete_window(){
    int wm_fd = open("dev/wm",FILE_FLAG_NONE);
    ioctl(wm_fd,DEV_WM_PROC_SHUTDOWN,0);
    close(wm_fd);
}

void write_pixel(user_fb_t* fb, uint32_t x, uint32_t y, uint32_t color){
    if (x >= fb->win_width || y >= fb->win_height) return;
    volatile uint32_t *fb32 = (volatile uint32_t*)fb->fb;
    fb32[y * fb->win_width + x] = color;
}

void write_char(user_fb_t* fb, uint8_t ch, uint32_t fg, uint32_t bg){
    if (ch < ' ' || ch > '~') return;
    const uint8_t map_idx = ch - ' ';

    uint32_t px = fb->cursor_x * COLUMNS_PER_CHAR;
    uint32_t py = fb->cursor_y * ROWS_PER_CHAR;

    for (uint32_t i = 0; i < ROWS_PER_CHAR; i++) {
        uint8_t row = char_bitmap_8x16[map_idx][i];
        for (uint8_t bit_idx = 0; bit_idx < 8; bit_idx++) {
            write_pixel(fb, px + (7 - bit_idx), py, (row & (1 << bit_idx)) ? fg : bg);
        }
        py++;
    }
}