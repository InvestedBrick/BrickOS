// handles the actual text printing / mapping of buffers
#include "cstdlib/syscalls.h"
#include "cstdlib/stdutils.h"
#include "cstdlib/malloc.h"
#include "cstdlib/stdio.h"
#include "../../kernel/src/filesystem/devices/device_defines.h"
#include "../../kernel/src/screen/font_bitmaps.h"
#include <stdint.h>

#define WINDOW_HEIGHT 400
#define WINDOW_WIDTH 650

static uint32_t term_cursor_x = 0;
static uint32_t term_cursor_y = 0;
static uint32_t term_cursor_x_max = WINDOW_WIDTH / COLUMNS_PER_CHAR;
static uint32_t term_cursor_y_max = WINDOW_HEIGHT / ROWS_PER_CHAR;

static unsigned char* term_fb = 0;
uint32_t kb_fd;

#define VBE_COLOR_BLACK 0xFF000000
#define VBE_COLOR_GRAY  0xFFAAAAAA

void term_write_pixel(uint32_t x, uint32_t y, uint32_t color){
    if (x >= WINDOW_WIDTH || y >= WINDOW_HEIGHT) return;
    volatile uint32_t *fb32 = (volatile uint32_t*)term_fb;
    fb32[y * WINDOW_WIDTH + x] = color;
}

void term_write_char(uint8_t ch, uint32_t fg, uint32_t bg){
    if (ch < ' ' || ch > '~') return;
    const uint8_t map_idx = ch - ' ';

    uint32_t px = term_cursor_x * COLUMNS_PER_CHAR;
    uint32_t py = term_cursor_y * ROWS_PER_CHAR;

    for (uint32_t i = 0; i < ROWS_PER_CHAR; i++) {
        uint8_t row = char_bitmap_8x16[map_idx][i];
        for (uint8_t bit_idx = 0; bit_idx < 8; bit_idx++) {
            term_write_pixel(px + (7 - bit_idx), py, (row & (1 << bit_idx)) ? fg : bg);
        }
        py++;
    }
    term_cursor_x++;
    if (term_cursor_x >= term_cursor_x_max) { term_cursor_x = 0; term_cursor_y++; }
    if (term_cursor_y >= term_cursor_y_max) term_cursor_y = term_cursor_y_max - 1;
}

void term_clear_screen(uint32_t color){
    for (uint32_t i = 0; i < WINDOW_HEIGHT; i++) {
        for (uint32_t j = 0; j < WINDOW_WIDTH; j++) {
            term_write_pixel(j, i, color);
        }
    }
    term_cursor_x = 0;
    term_cursor_y = 0;
}

void term_move_cursor_one_back(){
    if (term_cursor_x != 0) {
        term_cursor_x--;
    } else {
        if (term_cursor_y != 0) {
            term_cursor_x = term_cursor_x_max - 1;
            term_cursor_y--;
        }
    }
}

void term_update_cursor() {
    term_write_char(' ', VBE_COLOR_GRAY, VBE_COLOR_GRAY);
    term_move_cursor_one_back();
}

void term_erase_one_char(){
    term_write_char(' ', VBE_COLOR_BLACK, VBE_COLOR_BLACK);
    term_move_cursor_one_back();
    term_move_cursor_one_back();
    term_update_cursor();
}

void term_newline(){
    term_write_char(' ', VBE_COLOR_BLACK, VBE_COLOR_BLACK);
    if (term_cursor_y < term_cursor_y_max - 1) {
        term_cursor_y++;
        term_cursor_x = 0;
    }
    term_update_cursor();
}

void term_handle_input(unsigned char c){
    switch (c) {
        case '\n': {
            term_newline();
            break;
        }
        case '\e':{
            term_clear_screen(VBE_COLOR_BLACK);
            break;
        }
        case '\t':{
            term_cursor_x += 4;
            break;
        }
        case '\b': {
            term_erase_one_char();
            break;
        }
        default: {
            term_write_char(c, VBE_COLOR_GRAY, VBE_COLOR_BLACK);
            break;
        }
    }
    if (term_cursor_x >= term_cursor_x_max) { term_cursor_x = 0; term_cursor_y++; }
    if (term_cursor_y >= term_cursor_y_max) term_cursor_y = term_cursor_y_max - 1;
    term_update_cursor();
}

unsigned char* request_window(uint32_t width,uint32_t height){
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
    kb_fd = answer->kb_fd;
    unsigned char* fb = (unsigned char*)mmap(answer->width * answer->height * sizeof(uint32_t),PROT_READ | PROT_WRITE, MAP_SHARED,backing_fd,0);
    
    close(backing_fd);
    rmfile(answer->filename); // dispose of the connector
    chdir("../../..");
    free(pid_str);
    free(answer);
    close(wm_fd);

    return fb;
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

__attribute__((section(".text.start")))
void main(){
    int pid = getpid();
    unsigned char* pid_str = uint32_to_ascii((uint32_t)pid);
    uint32_t pid_strlen = strlen(pid_str);

    unsigned char* stdin = (unsigned char*)malloc(sizeof("tmp/stdin_") + pid_strlen);
    memcpy(stdin,"tmp/stdin_",sizeof("tmp/stdin_") - 1);
    memcpy(&stdin[sizeof("tmp/stdin_") - 1],pid_str,pid_strlen + 1);

    unsigned char* stdout = (unsigned char*)malloc(sizeof("tmp/stdout_") + pid_strlen);
    memcpy(stdout,"tmp/stdout_",sizeof("tmp/stdout_") - 1);
    memcpy(&stdout[sizeof("tmp/stdout_") - 1],pid_str,pid_strlen + 1);
    free(pid_str);

    term_fb = request_window(WINDOW_WIDTH,WINDOW_HEIGHT);
    if (!term_fb) exit(2);

    term_clear_screen(VBE_COLOR_BLACK);
    commit_window();

    mknod_params_t params = {
        .type = TYPE_PIPE,
        .flags = 0,
    };

    mknod(stdin,&params);
    mknod(stdout,&params);

    int stdin_fd = open(stdin,FILE_FLAG_WRITE);
    int stdout_fd = open(stdout,FILE_FLAG_READ);

    process_fds_init_t fds = {
        .stdin_filename = stdin,
        .stdout_filename = stdout,
        .stderr_filename = 0,
    };
    spawn("modules/shell.bin",0,&fds);
    unsigned char buf[256];

    while(1){
        int kb_n_bytes = read(kb_fd,buf,sizeof(buf));
        
        if (kb_n_bytes > 0) {
            write(stdin_fd,buf,kb_n_bytes);
        }
        int n = read(stdout_fd, buf, sizeof(buf));
        if (n < 0) {
            // shell exited
            delete_window();
            // all the pipes get closed when exiting
            break;
        }
        if (n > 0){
            for (int i = 0; i < n; i++) {
                term_handle_input(buf[i]);
            }
            commit_window();
        }

    }

    exit(0);
}