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

typedef struct window {
    uint32_t origin_x;
    uint32_t origin_y;
    uint32_t width;
    uint32_t height;
    uint32_t flags;
    uint32_t z;
    uint32_t owner_pid;
    uint32_t kb_pipe_fd;
    int backing_fd;
    uint32_t buffer_size;
    unsigned char* live_buffer;
    unsigned char* comitted_buffer;
    struct window* next;
}window_t;

window_t* head = 0; 
window_t* focused_window = 0;

window_t* find_window_by_z(uint32_t z){
    window_t* current = head;
    while(current){
        if (current->z == z) return current;
        current = current->next;
    }   
    return 0;
}

window_t* get_prev_window(window_t* win){
    window_t* current = head;
    if (current == win) return 0; 

    while(current){
        if (current->next == win) return current;
        current = current->next;
    }   
    return 0;
}

uint32_t find_largest_z(){
    window_t* current = head;
    uint32_t largest_z = 0;
    while (current) {
        if (current->z > largest_z) largest_z = current->z;
        current = current->next; 
    }

    return largest_z;
}

uint32_t find_lowest_z(){
    window_t* current = head;
    uint32_t lowest_z = (uint32_t)-1;
    while (current) {
        if (current->z < lowest_z) lowest_z = current->z;
        current = current->next; 
    }

    return lowest_z;
}

window_t* find_window_with_next_larger_z(uint32_t current_z){
    uint32_t best_z = (uint32_t)-1;
    window_t* current = head;

    while (current){
        if (current->z > current_z && current->z < best_z) best_z = current->z;
        current = current->next;
    }

    return find_window_by_z(best_z);
}
unsigned char* fb0 = 0;
uint32_t screen_bytes_per_row = 0;
uint32_t screen_bytespp = 0;
int wm_to_k_fd = 0;
int k_to_wm_fd = 0;
uint8_t window_dirty = 0;
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

    unsigned char* pid_str = uint32_to_ascii(msg->pid);
    int dir = open(pid_str,FILE_FLAG_CREATE | FILE_CREATE_DIR);
    chdir(pid_str);

    mknod_params_t params = {
        .type = TYPE_PIPE,
        .flags = MNODE_FLAG_PID_DEFINED_PIPE,
        .read_pid = new_win->owner_pid,
        .write_pid = getpid()
    };

    mknod("kb_pipe",&params); // replaces read and write pids with the opened fds
    new_win->kb_pipe_fd = params.write_pid;

    new_win->buffer_size = new_win->width * new_win->height * sizeof(uint32_t);
    new_win->next = 0;
    new_win->z = find_largest_z() + 1;
    new_win->origin_x = (fb_metadata->width / 5);
    new_win->origin_y = (fb_metadata->height / 5);
    uint32_t filename_len = sizeof("win.tmp");
    unsigned char* backing_filename = (unsigned char*)malloc(filename_len);
    memcpy(backing_filename,"win.tmp",sizeof("win.tmp"));
    free(pid_str);

    new_win->backing_fd = open(backing_filename,FILE_FLAG_CREATE | FILE_FLAG_READ | FILE_FLAG_WRITE);
    
    new_win->live_buffer = (unsigned char*)mmap(new_win->buffer_size,PROT_READ | PROT_WRITE,MAP_SHARED,new_win->backing_fd,0);
    new_win->comitted_buffer = (unsigned char*)malloc(new_win->buffer_size);
    // be sure that all pages are present, so that when the user process maps it, the file can be isntantly deleted (not good practise but eh, probably only temporary solution)
    memset(new_win->live_buffer,0,new_win->buffer_size);
    close(new_win->backing_fd);
    
    window_creation_ans_t creation_ans = {
        .answer_type = DEV_WM_ANS_TYPE_WIN_CREATION,
        .pid = new_win->owner_pid,
        .width = new_win->width,
        .height = new_win->height, 
        .filename_len = filename_len,
        .kb_fd = params.read_pid,
        };
    // always pass the answer type first
    write(wm_to_k_fd,(unsigned char*)&creation_ans,sizeof(window_creation_ans_t)); 
    write(wm_to_k_fd,backing_filename,filename_len);
    
    add_window_to_list(new_win);
    free(backing_filename);
    chdir("..");
}

void handle_window_commit(framebuffer_t* fb_metadata,win_man_msg_t* msg){
    window_t* win = find_window_by_pid(msg->pid);
    if (!win) return;

    memcpy(win->comitted_buffer,win->live_buffer,win->buffer_size);
    window_dirty = 1;
}

void write_window_to_fb(framebuffer_t* fb_metadata, window_t* win) {
    unsigned char* win_start = (unsigned char*)((uint64_t)fb0 + win->origin_y * fb_metadata->bytes_per_row + win->origin_x * screen_bytespp);

    uint32_t width_copy_size = ((win->origin_x + win->width > fb_metadata->width) ? (fb_metadata->width - win->origin_x) : win->width) * screen_bytespp;
    uint32_t rows_to_copy = (win->origin_y + win->height > fb_metadata->height) ? (fb_metadata->height - win->origin_y) : win->height;

    if (!win_start || width_copy_size == 0 || rows_to_copy == 0) return;

    if (win->origin_y > 0) {
        unsigned char* top_border = win_start - fb_metadata->bytes_per_row;
        memset(top_border, 0xAA, width_copy_size);
    }

    uint32_t buffer_cpy_off = 0;
    for (uint32_t i = 0; i < rows_to_copy; i++) {
        if (win->origin_x > 0) {
            memset(win_start - screen_bytespp, 0xAA, screen_bytespp);
        }

        memcpy(win_start, &win->comitted_buffer[buffer_cpy_off], width_copy_size);

        if (win->origin_x + win->width < fb_metadata->width) {
            memset(win_start + width_copy_size, 0xAA, screen_bytespp);
        }

        win_start += fb_metadata->bytes_per_row;
        buffer_cpy_off += width_copy_size;
    }

    if (win->origin_y + rows_to_copy < fb_metadata->height) {
        memset(win_start, 0xAA, width_copy_size);
    }
}


void composite_windows(framebuffer_t* fb_metadata){
    if (!head) return;
    if (!window_dirty) return;
    
    uint32_t current_z = 0; // 0 is guaranteed to not be a Z-layer
    while(1){
        window_t* win = find_window_with_next_larger_z(current_z);
        if (!win) break;

        write_window_to_fb(fb_metadata,win);
        current_z = win->z;
    }

    uint32_t max_z = find_largest_z();
    focused_window = find_window_by_z(max_z);

    window_dirty = 0;
}

void handle_process_shudown(uint32_t pid){
    window_t* win = find_window_by_pid(pid);

    if (!win) return;

    unsigned char* pid_str = uint32_to_ascii(pid);

    free(win->comitted_buffer);
    chdir(pid_str);
    close(win->kb_pipe_fd);
    //rmfile("kb_pipe"); pipe closes automatically when the user proc closes the pipe
    chdir("..");

    rmfile(pid_str);

    //TODO: munmap

    window_t* prev = get_prev_window(win);
    prev->next = win->next;
    free(win);
    free(pid_str);

}

__attribute__((section(".text.start")))
void main(){
    int fb0_fd = open("dev/fb0",FILE_FLAG_NONE);
    int kb0_fd = open("dev/kb0",FILE_FLAG_NONE);
    wm_to_k_fd = open("tmp/wm_to_k.tmp",FILE_FLAG_WRITE);
    k_to_wm_fd = open("tmp/k_to_wm.tmp",FILE_FLAG_READ);

    chdir("tmp");
    int wm_dir = open("wm",FILE_FLAG_CREATE | FILE_CREATE_DIR);
    if (wm_dir < 0) exit(1);
    close(wm_dir);
    chdir("wm");

    if (fb0_fd < 0) exit(2);
    if (kb0_fd < 0) exit(2);
    if (wm_to_k_fd < 0) exit(2);
    if (k_to_wm_fd < 0) exit(2);
    framebuffer_t fb_metadata;
    if (ioctl(fb0_fd,DEV_FB0_GET_METADATA,&fb_metadata) < 0) exit(3);
    
    screen_bytes_per_row = fb_metadata.bytes_per_row;
    screen_bytespp = fb_metadata.bpp / 8;

    fb0 = (unsigned char*)mmap(fb_metadata.size, PROT_READ | PROT_WRITE, MAP_SHARED, fb0_fd,0);
    
    close(fb0_fd);
    if (!fb0) exit(4);
    
    unsigned char buffer[256] = {0};
    
    uint32_t n_input = 0;
    uint32_t n_kmsg = 0;
    while(1){
        n_input = read(kb0_fd,buffer,sizeof(buffer));
        if (focused_window && n_input > 0){
            write(focused_window->kb_pipe_fd,buffer,n_input);
        }

        memset(buffer,0,sizeof(buffer));

        n_kmsg = read(k_to_wm_fd,buffer,sizeof(buffer));
        if (n_kmsg > 0){

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
                case DEV_WM_PROC_SHUTDOWN: {
                    handle_process_shudown(msg->pid);
                    break;
                }
                default: 
                    break;
            }
        }
        composite_windows(&fb_metadata);

    }

    exit(EXIT_SUCCESS);
}
