#include "cstdlib/stdutils.h"
#include "cstdlib/syscalls.h"
#include "cstdlib/stdio.h"
#include "../../kernel/src/filesystem/devices/device_defines.h"
#include "cstdlib/malloc.h"

#define MAX_MOUSE_PACKETS 128

typedef struct {
    uint32_t cmd;
    uint32_t pid;
    uint32_t size;
    unsigned char data[];    
}win_man_msg_t;

typedef struct {
    uint32_t x,y;
    uint32_t width,height;
}section_t;

typedef struct window {
    struct window* next;
    section_t section;
    uint32_t flags;
    uint32_t owner_pid;
    uint32_t kb_pipe_fd;
    int backing_fd;
    uint32_t buffer_size;
    unsigned char* live_buffer;
    unsigned char* comitted_buffer;
    uint32_t visit_id;
    uint8_t dirty;
}window_t;

uint32_t current_visit_id = 1;

typedef struct {
    uint32_t posx;
    uint32_t posy;

    uint32_t old_posx;
    uint32_t old_posy;

    const char icon[3][3];

    unsigned char overriden_data[3][3][4]; // for when the mouse is over a window, so that we can restore the pixels when the mouse moves away
    
    uint8_t left_clicked;
    uint8_t right_clicked;
    uint8_t middle_clicked;
}mouse_t;

window_t* head = 0; 
window_t* focused_window = 0;

section_t dirty_sections[MAX_MOUSE_PACKETS];
uint32_t dirty_section_idx = 0;

section_t dirty_cluster[64]; 
uint32_t dirty_cluster_index = 0;

window_t* get_prev_window(window_t* win){
    window_t* iter = head;
    if (iter == win) return 0; 

    while(iter){
        if (iter->next == win) return iter;
        iter = iter->next;
    }   
    return 0;
}

unsigned char* back_buffer = 0;
unsigned char* fb0 = 0;
uint32_t screen_bytes_per_row = 0;
uint32_t screen_bytespp = 0;
int wm_to_k_fd = 0;
int k_to_wm_fd = 0;
uint32_t n_windows;


mouse_t mouse = {
    .posx = 0,
    .posy = 0,
    .old_posx = 0,
    .old_posy = 0,
    .icon = {
        {0xff,0xff,0xff},
        {0xff,0xff,0xff},
        {0xff,0xff,0xff}
    },
    .overriden_data = {0},
    .left_clicked = 0,
    .right_clicked = 0,
    .middle_clicked = 0
};

section_t screen_moved_sec = {0};

void add_window_to_list(window_t* new_win){
    window_t* iter = head;
    if (!iter) head = new_win;
    else {
        while(iter->next) iter = iter->next;
        iter->next = new_win;
    }
}

window_t* find_window_by_pid(uint32_t pid){
    window_t* iter = head;
    while(iter){
        if (iter->owner_pid == pid) return iter;
        iter = iter->next;
    }

    return 0;
}   

window_t* find_prev_window(window_t* win){
    window_t* iter = head;
    if (iter == win) return 0;
    while(iter->next && iter->next != win) iter = iter->next;

    return (iter->next == win) ? iter : 0;
}

uint8_t sections_intersecting(section_t* sec_a, section_t* sec_b){
    return (sec_a->x < sec_b->x +      sec_b->width &&
       sec_a->x + sec_a->width >  sec_b->x &&
       sec_a->y < sec_b->y +      sec_b->height &&
       sec_a->y + sec_a->height > sec_b->y);
}

void handle_window_request(framebuffer_t* fb_metadata,win_man_msg_t* msg){
    window_req_t* req = (window_req_t*)msg->data;
    if (req->height > fb_metadata->height) req->height = fb_metadata->height;
    if (req->width > fb_metadata->width) req->width = fb_metadata->width;
    window_t* new_win = (window_t*)malloc(sizeof(window_t));
    new_win->dirty = 1;
    new_win->flags = req->flags;
    new_win->section.width = req->width;
    new_win->section.height = req->height;
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

    new_win->buffer_size = new_win->section.width * new_win->section.height * sizeof(uint32_t);
    new_win->next = 0;
    new_win->section.x = n_windows * 50;
    new_win->section.y = n_windows * 50;
    dirty_cluster[dirty_cluster_index++] = new_win->section;
    n_windows++;
    uint32_t filename_len = sizeof("win.tmp");
    unsigned char* backing_filename = (unsigned char*)malloc(filename_len);
    memcpy(backing_filename,"win.tmp",sizeof("win.tmp"));
    free(pid_str);

    new_win->backing_fd = open(backing_filename,FILE_FLAG_CREATE | FILE_FLAG_READ | FILE_FLAG_WRITE);
    
    new_win->live_buffer = (unsigned char*)mmap(new_win->buffer_size,PROT_READ | PROT_WRITE,MAP_SHARED,new_win->backing_fd,0);
    new_win->comitted_buffer = (unsigned char*)malloc(new_win->buffer_size);
    // be sure that all pages are present, so that when the user process maps it, the file can be isntantly deleted (not good practise but eh, probably only temporary solution)
    memset(new_win->live_buffer,0,new_win->buffer_size);
    memset(new_win->comitted_buffer,0,new_win->buffer_size);
    close(new_win->backing_fd);
    
    window_creation_ans_t creation_ans = {
        .answer_type = DEV_WM_ANS_TYPE_WIN_CREATION,
        .pid = new_win->owner_pid,
        .width = new_win->section.width,
        .height = new_win->section.height, 
        .filename_len = filename_len,
        .kb_fd = params.read_pid,
        };
    // always pass the answer type first
    write(wm_to_k_fd,(unsigned char*)&creation_ans,sizeof(window_creation_ans_t)); 
    write(wm_to_k_fd,backing_filename,filename_len);
    
    add_window_to_list(new_win);
    focused_window = new_win;
    free(backing_filename);
    chdir("..");
}

uint32_t min(uint32_t a, uint32_t b) {
    return (a < b) ? a : b;
}
uint32_t max(uint32_t a, uint32_t b) {
    return (a > b) ? a : b;
}

void enlarge_section(section_t* sec,uint32_t x, uint32_t y,uint32_t width, uint32_t height){
    if (sec->width == 0 && sec->height == 0){
        // initialize
        sec->x = x;
        sec->y = y;
        sec->width = width;
        sec->height = height;
        return;
    }

    uint32_t new_left   = min(sec->x,x);
    uint32_t new_top    = min(sec->y,y);
    uint32_t new_right  = max(sec->x + sec->width, x + width);
    uint32_t new_bottom = max(sec->y + sec->height, y + height);

    sec->x = new_left;
    sec->y = new_top;
    sec->width = new_right - new_left;
    sec->height = new_bottom - new_top; 
}

section_t create_composite_section(section_t secs[],uint32_t n_sections){
    if (n_sections == 0) return (section_t){0};

    uint32_t lowest_x  = secs[0].x;
    uint32_t lowest_y  = secs[0].y;
    uint32_t highest_x = secs[0].x + secs[0].width;
    uint32_t highest_y = secs[0].y + secs[0].height;

    for (uint32_t i = 0; i < n_sections;i++){
        section_t sec = secs[i];
        if (i == 0 || sec.x < lowest_x) lowest_x = sec.x;
        if (i == 0 || sec.y < lowest_y) lowest_y = sec.y;
        if (i == 0 || sec.x + sec.width > highest_x) highest_x = sec.x + sec.width;
        if (i == 0 || sec.y + sec.height > highest_y) highest_y = sec.y + sec.height;
    }

    section_t sec = {
        .x = lowest_x,
        .y = lowest_y,
        .width = highest_x - lowest_x,
        .height = highest_y - lowest_y,
    };

    return sec;
}

void mark_intersecting_window_dirty_rec(window_t* start, window_t* target,uint8_t record)
{
    if (target->visit_id == current_visit_id) return; // already visited
    
    target->visit_id = current_visit_id;

    window_t* iter = start;
    while (iter) {
        if (sections_intersecting(&iter->section, &target->section))
        {   
            iter->dirty = 1;
            
            if (record && dirty_section_idx < MAX_MOUSE_PACKETS)
                dirty_sections[dirty_section_idx++] = iter->section;

            // propagate upwards
            mark_intersecting_window_dirty_rec(iter->next, iter,record);
        }

        iter = iter->next;
    }
}


void mark_higher_intersecting_windows_dirty(window_t* win,uint8_t record)
{
    current_visit_id++;
    mark_intersecting_window_dirty_rec(win->next, win,record);
}

void mark_intersecting_windows_dirty(window_t* win,uint8_t record)
{
    current_visit_id++;
    mark_intersecting_window_dirty_rec(head, win,record);
}


void handle_window_commit(framebuffer_t* fb_metadata,win_man_msg_t* msg){
    window_t* win = find_window_by_pid(msg->pid);
    if (!win) return;

    memcpy(win->comitted_buffer,win->live_buffer,win->buffer_size);
    win->dirty = 1;
    dirty_section_idx = 0;
    dirty_sections[dirty_section_idx++] = win->section;
    mark_higher_intersecting_windows_dirty(win,1);
    section_t cluster = create_composite_section(dirty_sections,dirty_section_idx);
    dirty_section_idx = 0;
    dirty_cluster[dirty_cluster_index++] = cluster;
}

void write_window_to_fb(framebuffer_t* fb_metadata, window_t* win) {
    unsigned char* win_start = (unsigned char*)((uint64_t)back_buffer + win->section.y * fb_metadata->bytes_per_row + win->section.x * screen_bytespp);

    uint32_t width_copy_size = ((win->section.x + win->section.width > fb_metadata->width) ? (fb_metadata->width - win->section.x) : win->section.width) * screen_bytespp;
    uint32_t rows_to_copy = (win->section.y + win->section.height > fb_metadata->height) ? (fb_metadata->height - win->section.y) : win->section.height;

    if (!win_start || width_copy_size == 0 || rows_to_copy == 0) return;

    if (win->section.y > 0) {
        unsigned char* top_border = win_start - fb_metadata->bytes_per_row;
        memset(top_border, 0xAA, width_copy_size);
    }

    uint32_t buffer_cpy_off = 0;
    for (uint32_t i = 0; i < rows_to_copy; i++) {
        if (win->section.x > 0) {
            memset(win_start - screen_bytespp, 0xAA, screen_bytespp);
        }

        memcpy(win_start, &win->comitted_buffer[buffer_cpy_off], width_copy_size);

        if (win->section.x + win->section.width < fb_metadata->width) {
            memset(win_start + width_copy_size, 0xAA, screen_bytespp);
        }

        win_start += fb_metadata->bytes_per_row;
        buffer_cpy_off += win->section.width * screen_bytespp;
    }

    if (win->section.y + rows_to_copy < fb_metadata->height) {
        memset(win_start, 0xAA, width_copy_size);
    }
}

void clear_section(section_t sec, framebuffer_t* fb_metadata){
    if (sec.x >= fb_metadata->width || sec.y >= fb_metadata->height) return;
    
    if (sec.x + sec.width > fb_metadata->width)   sec.width  = fb_metadata->width  - sec.x;
    if (sec.y + sec.height > fb_metadata->height) sec.height = fb_metadata->height - sec.y;

    for (uint32_t j = 0; j < sec.height; j++){
        unsigned char* row_start = back_buffer + (sec.y + j) * fb_metadata->bytes_per_row + sec.x * screen_bytespp;
        memset(row_start, 0, sec.width * screen_bytespp);
    }
}


void blit_section(section_t sec, framebuffer_t* fb_metadata) {
    for (uint32_t y = 0;y < sec.height;y++) {
        memcpy(
            fb0 + (sec.y+y)*fb_metadata->bytes_per_row + sec.x*screen_bytespp,
            back_buffer + (sec.y+y)*fb_metadata->bytes_per_row + sec.x*screen_bytespp,
            sec.width * screen_bytespp
        );
    }
}



void composite_windows(framebuffer_t* fb_metadata){
    if (!head) return;
    uint8_t moved = 0;
    if (screen_moved_sec.width != 0 || screen_moved_sec.height != 0){
        moved = 1;
        if (screen_moved_sec.x > 0) screen_moved_sec.x--;
        if (screen_moved_sec.y > 0) screen_moved_sec.y--;
        screen_moved_sec.width  += 2;
        screen_moved_sec.height += 2;
        clear_section(screen_moved_sec,fb_metadata);
    }
    
    window_t* iter = head;
    while(iter){
        
        if (iter->dirty){
            write_window_to_fb(fb_metadata,iter);
            iter->dirty = 0;
        }
        iter = iter->next;
    }
    
    if (moved){
        blit_section(screen_moved_sec,fb_metadata);
    }
    for (uint32_t i = 0; i < dirty_cluster_index;i++){
        if (dirty_cluster[i].x > 0) dirty_cluster[i].x--;
        if (dirty_cluster[i].y > 0) dirty_cluster[i].y--;

        dirty_cluster[i].width  += 2;
        dirty_cluster[i].height += 2;

        blit_section(dirty_cluster[i],fb_metadata);
    }

    dirty_cluster_index = 0;
    dirty_section_idx = 0;
    memset(&screen_moved_sec,0x0,sizeof(screen_moved_sec));
    
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
    if (prev)
        prev->next = win->next;
    if (win == focused_window){
        prev = focused_window;
    }

    // clean it up
    enlarge_section(&screen_moved_sec,win->section.x,win->section.y,win->section.width,win->section.height);
    mark_intersecting_windows_dirty(win,0);
    free(win);
    win = 0;
    free(pid_str);

}

// draw a 3x3 square for the mouse pointer
void draw_mouse(framebuffer_t* fb_metadata){
    unsigned char* mouse_start = (unsigned char*)((uint64_t)fb0 + mouse.posy * fb_metadata->bytes_per_row + mouse.posx * screen_bytespp);
    // draw mouse icon and save the pixels that are being overridden
    for (uint16_t x = 0; x < sizeof(mouse.icon[0]); x++){
        for (uint16_t y = 0; y < sizeof(mouse.icon) / sizeof(mouse.icon[0]); y++){
            unsigned char* pixel = mouse_start + y * fb_metadata->bytes_per_row + x * screen_bytespp;
            if (pixel < fb0 || pixel >= fb0 + fb_metadata->size) continue; // sanity check to avoid writing out of bounds
            
            memcpy(mouse.overriden_data[y][x],pixel,screen_bytespp);
            
            memset(pixel,mouse.icon[y][x],screen_bytespp);
        }
    }
}

void restore_mouse_underlying_pixels(framebuffer_t* fb_metadata){
    unsigned char* mouse_start = (unsigned char*)((uint64_t)fb0 + mouse.old_posy * fb_metadata->bytes_per_row + mouse.old_posx * screen_bytespp);
    
    for (uint16_t x = 0; x < sizeof(mouse.icon[0]); x++){
        for (uint16_t y = 0; y < sizeof(mouse.icon) / sizeof(mouse.icon[0]); y++){
            unsigned char* pixel = mouse_start + y * fb_metadata->bytes_per_row + x * screen_bytespp;
            if (pixel < fb0 || pixel >= fb0 + fb_metadata->size) continue; 
            
            memcpy(pixel,mouse.overriden_data[y][x],screen_bytespp);
        }
    }
}

window_t* get_mouse_top_window(){
    window_t* iter = head;
    window_t* win = 0;
    while(iter){
        if (mouse.posx >= iter->section.x &&
            mouse.posx <= iter->section.x + iter->section.width &&
            mouse.posy >= iter->section.y &&
            mouse.posy <= iter->section.y + iter->section.height 
        )
        {
            win = iter; // is guaranteed to be higher because we iterate from lowest -> highest
        }
        iter = iter->next;
    }

    return win; 
}

void adjust_focused_window()
{
    if (!focused_window || !head)
        return;

    if (head->next == 0)
        return;

    if (focused_window->next == 0)
        return;

    window_t *prev = find_prev_window(focused_window);

    if (prev) {
        prev->next = focused_window->next;
    } else {
        head = focused_window->next;
    }

    window_t *tail = head;
    while (tail->next)
        tail = tail->next;

    tail->next = focused_window;
    focused_window->next = 0;
}


void move_window(window_t* win,framebuffer_t* fb_metadata,int16_t relx, int16_t rely){
    int32_t new_x = (int32_t)win->section.x + relx;
    int32_t new_y = (int32_t)win->section.y - rely;

    if (new_x < 0) new_x = 0;
    if (new_x >= (int32_t)fb_metadata->width) new_x = fb_metadata->width - 1;
    if (new_y < 0) new_y = 0;
    if (new_y >= (int32_t)fb_metadata->height) new_y = fb_metadata->height - 1;


    if (relx != 0 || rely != 0){
        mark_intersecting_windows_dirty(win,0); 
        //save the old section to be deleted
        enlarge_section(&screen_moved_sec,win->section.x,win->section.y,win->section.width,win->section.height);

        // also save new one so that borders all render
        enlarge_section(&screen_moved_sec,new_x,new_y,win->section.width,win->section.height);
    }

    win->section.x = (uint32_t)new_x;
    win->section.y = (uint32_t)new_y;
}

void update_mouse(framebuffer_t* fb_metadata,int mouse_fd){
    mouse_packet_t buffer[MAX_MOUSE_PACKETS];
    int bytes_read = read(mouse_fd,(const char*)buffer,sizeof(buffer));
    uint32_t n_packets = bytes_read / sizeof(mouse_packet_t);
    for (uint32_t i = 0; i < n_packets;i++){

        int16_t rel_x = buffer[i].relx;
        int16_t rel_y = buffer[i].rely;  
        
        if (buffer[i].btns & MOUSE_BTN_LEFT_MASK) {
            window_t* win = get_mouse_top_window();
            if (win) {
                focused_window = win;
                adjust_focused_window();
                if (!win->dirty){
                    win->dirty = 1;
                    dirty_section_idx = 0;
                    dirty_sections[dirty_section_idx++] = win->section;
                    mark_higher_intersecting_windows_dirty(win,1);
                    section_t cluster = create_composite_section(dirty_sections,dirty_section_idx);
                    dirty_section_idx = 0;
                    dirty_cluster[dirty_cluster_index++] = cluster;
                }
                if (mouse.left_clicked) {
                    move_window(win, fb_metadata, rel_x, rel_y);
            
                } else {
                    mouse.left_clicked = 1;
                }
            }
        } else {
            mouse.left_clicked = 0;
        }

        int32_t new_x = (int32_t)mouse.posx + rel_x;
        int32_t new_y = (int32_t)mouse.posy - rel_y;
        
        if (new_x < 0) new_x = 0;
        if (new_x >= (int32_t)fb_metadata->width) new_x = fb_metadata->width - 1;
        if (new_y < 0) new_y = 0;
        if (new_y >= (int32_t)fb_metadata->height) new_y = fb_metadata->height - 1;
        
        mouse.old_posx = mouse.posx;
        mouse.old_posy = mouse.posy;

        mouse.posx = (uint32_t)new_x;
        mouse.posy = (uint32_t)new_y;
        restore_mouse_underlying_pixels(fb_metadata);
        draw_mouse(fb_metadata);
        
    }
}

__attribute__((section(".text.start")))
void main(){
    int fb0_fd = open("dev/fb0",FILE_FLAG_NONE);
    int kb0_fd = open("dev/kb0",FILE_FLAG_NONE);
    int mouse_fd = open("dev/mouse",FILE_FLAG_NONE);
    debug("BER");
    debug(uint32_to_ascii((uint32_t)&wm_to_k_fd)); // TODO: magic hotfix, need to investigate
    wm_to_k_fd = open("tmp/wm_to_k.tmp",FILE_FLAG_WRITE);
    debug("AFTER");
    k_to_wm_fd = open("tmp/k_to_wm.tmp",FILE_FLAG_READ);

    chdir("tmp");
    int wm_dir = open("wm",FILE_FLAG_CREATE | FILE_CREATE_DIR);
    if (wm_dir < 0) exit(1);
    close(wm_dir);
    chdir("wm");

    if (fb0_fd < 0) exit(2);
    if (kb0_fd < 0) exit(2);
    if (mouse_fd < 0) exit(2);
    if (wm_to_k_fd < 0) exit(2);
    if (k_to_wm_fd < 0) exit(2);

    
    framebuffer_t fb_metadata;
    if (ioctl(fb0_fd,DEV_FB0_GET_METADATA,&fb_metadata) < 0) exit(3);
    screen_bytes_per_row = fb_metadata.bytes_per_row;
    screen_bytespp = fb_metadata.bpp / 8;
    
    fb0 = (unsigned char*)mmap(fb_metadata.size, PROT_READ | PROT_WRITE, MAP_SHARED, fb0_fd,0);
    back_buffer = (unsigned char*)mmap(fb_metadata.size,PROT_READ | PROT_WRITE,MAP_ANON,0,0);
    
    close(fb0_fd);
    if (!fb0) exit(4);
    
    mouse.posx = fb_metadata.width / 2;
    mouse.posy = fb_metadata.height / 2;
    
    unsigned char buffer[256];
    memset(buffer,0,sizeof(buffer));
    
    uint32_t n_input = 0;
    uint32_t n_kmsg = 0;
    while(1){
        n_input = read(kb0_fd,buffer,sizeof(buffer));
        if (focused_window && n_input > 0){
            write(focused_window->kb_pipe_fd,buffer,n_input);
        }

        memset(buffer,0,sizeof(buffer));
        
        n_kmsg = read(k_to_wm_fd,buffer,sizeof(buffer));
        uint32_t bytes_read = 0;
        
        while (bytes_read < n_kmsg){
            
            win_man_msg_t* msg = (win_man_msg_t*)&buffer[bytes_read];
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
            bytes_read += msg->size;
        }

        composite_windows(&fb_metadata);
        update_mouse(&fb_metadata,mouse_fd);
    }

    exit(EXIT_SUCCESS);
}
