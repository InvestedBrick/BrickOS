// handles the actual text printing / mapping of buffers
#include "cstdlib/syscalls.h"
#include "cstdlib/stdutils.h"
#include "cstdlib/malloc.h"
#include "cstdlib/stdio.h"
#include "cstdlib/window.h"
#include <stdint.h>

#define REQ_WIDTH  650
#define REQ_HEIGHT 400

#define VBE_COLOR_BLACK 0xff000000
#define VBE_COLOR_GRAY  0xffaaaaaa

void scroll_up(user_fb_t* fb){
    if (fb->win_height == 0 || fb->win_width == 0) return;

    uint32_t bytes_per_row = fb->win_width * sizeof(uint32_t);
    uint32_t rows_to_move = ROWS_PER_CHAR;

    uint32_t dest_rows = fb->win_height - rows_to_move;

    for (uint32_t r = 0; r < dest_rows; r++) {
        unsigned char* dst = fb->fb + (r * bytes_per_row);
        unsigned char* src = fb->fb + ((r + rows_to_move) * bytes_per_row);
        memcpy(dst, src, bytes_per_row);
    }

    unsigned char* clear_start = fb->fb + (dest_rows * bytes_per_row);
    memset(clear_start, 0, rows_to_move * bytes_per_row);

    if (fb->cursor_y > 0) fb->cursor_y--;
}

void term_write_char(user_fb_t* fb, uint8_t ch, uint32_t fg, uint32_t bg){
    write_char(fb, ch, fg, bg);
    fb->cursor_x++;
    if (fb->cursor_x >= fb->cursor_x_max) { fb->cursor_x = 0; fb->cursor_y++; }
    if (fb->cursor_y >= fb->cursor_y_max) {
        scroll_up(fb);
    }
}

void term_clear_screen(user_fb_t* fb, uint32_t color){
    for (uint32_t i = 0; i < fb->win_height; i++) {
        for (uint32_t j = 0; j < fb->win_width; j++) {
            write_pixel(fb, j, i, color);
        }
    }
    fb->cursor_x = 0;
    fb->cursor_y = 0;
}

void term_move_cursor_one_back(user_fb_t* fb){
    if (fb->cursor_x != 0) {
        fb->cursor_x--;
    } else {
        if (fb->cursor_y != 0) {
            fb->cursor_x = fb->cursor_x_max - 1;
            fb->cursor_y--;
        }
    }
}

void term_update_cursor(user_fb_t* fb) {
    term_write_char(fb, ' ', VBE_COLOR_GRAY, VBE_COLOR_GRAY);
    term_move_cursor_one_back(fb);
}

void term_erase_one_char(user_fb_t* fb){
    term_write_char(fb,' ', VBE_COLOR_BLACK, VBE_COLOR_BLACK);
    term_move_cursor_one_back(fb);
    term_move_cursor_one_back(fb);
    term_update_cursor(fb);
}

void term_newline(user_fb_t* fb){
    term_write_char(fb, ' ', VBE_COLOR_BLACK, VBE_COLOR_BLACK);
    fb->cursor_y++;
    fb->cursor_x = 0;
    if (fb->cursor_y >= fb->cursor_y_max) {
        scroll_up(fb);
    }
    term_update_cursor(fb);
}

void term_handle_input(user_fb_t* fb, unsigned char c){
    switch (c) {
        case '\n': {
            term_newline(fb);
            break;
        }
        case '\e':{
            term_clear_screen(fb, VBE_COLOR_BLACK);
            break;
        }
        case '\t':{
            fb->cursor_x += 4;
            break;
        }
        case '\b': {
            term_erase_one_char(fb);
            break;
        }
        default: {
            term_write_char(fb, c, VBE_COLOR_GRAY, VBE_COLOR_BLACK);
            break;
        }
    }
    if (fb->cursor_x >= fb->cursor_x_max) { fb->cursor_x = 0; fb->cursor_y++; }
    if (fb->cursor_y >= fb->cursor_y_max) fb->cursor_y = fb->cursor_y_max - 1;
    term_update_cursor(fb);
}
typedef struct{
    int stdout_fd;
    int stdin_fd;
    unsigned char* stdout_filename; 
    unsigned char* stdin_filename;
}pipe_return_t;

pipe_return_t create_io_pipes(){
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

    mknod_params_t params = {
        .type = TYPE_PIPE,
        .flags = 0,
    };

    mknod(stdin,&params);
    mknod(stdout,&params);

    int stdin_fd = open(stdin,FILE_FLAG_WRITE);
    int stdout_fd = open(stdout,FILE_FLAG_READ);

    pipe_return_t ret;
    ret.stdin_fd = stdin_fd;
    ret.stdin_filename = stdin;
    ret.stdout_fd = stdout_fd;
    ret.stdout_filename = stdout;

    return ret;
}

void main(){
    user_fb_t term_fb;
    request_window(&term_fb,REQ_WIDTH,REQ_HEIGHT);
    if (!term_fb.fb) exit(2);

    term_clear_screen(&term_fb, VBE_COLOR_BLACK);
    commit_window();

    pipe_return_t ret = create_io_pipes();

    process_fds_init_t fds = {
        .stdin_filename = ret.stdin_filename,
        .stdout_filename = ret.stdout_filename,
        .stderr_filename = 0,
    };
    spawn("modules/shell.elf",0,&fds);
    unsigned char buf[256];

    while(1){
        int kb_n_bytes = read(term_fb.kb_fd,buf,sizeof(buf));
        
        if (kb_n_bytes > 0) {
            write(ret.stdin_fd,buf,kb_n_bytes);
        }
        int n = read(ret.stdout_fd, buf, sizeof(buf));
        if (n < 0) {
            // shell exited
            delete_window();
            // all the pipes get closed when exiting
            break;
        }
        if (n > 0){
            for (int i = 0; i < n; i++) {
                term_handle_input(&term_fb, buf[i]);
            }
            commit_window();
        }

    }
    exit(0);
}