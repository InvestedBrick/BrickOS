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

typedef struct {
    section_t sections[5];
}section_split_t;

#define MAX_DIRTY_SECTIONS (0x4000 / sizeof(section_t))

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
}window_t;

typedef struct {
    uint32_t posx;
    uint32_t posy;

    const char icon[3][3];

    uint8_t left_clicked;
    uint8_t right_clicked;
    uint8_t middle_clicked;

    uint8_t tried_window; // used to only try to click a window once per mouse down event
    window_t* dragging_window; 
}mouse_t;

window_t* head = 0; 
section_t* dirty_sections = 0;

#define SECTION_EXPANDED_TOP 0x1
#define SECTION_EXPANDED_LEFT 0x2
#define SECTION_EXPANDED_BOTTOM 0x4
#define SECTION_EXPANDED_RIGHT 0x8

uint8_t dirty_section_expanded[MAX_DIRTY_SECTIONS] = {0};
uint32_t dirty_section_idx = 0;


void set_section_expanded(uint32_t idx, uint8_t expanded){
    if (idx >= MAX_DIRTY_SECTIONS) return;
    dirty_section_expanded[idx] |= expanded;
}
uint8_t sections_identical(const section_t* a, const section_t* b){
    return (a->x == b->x && a->y == b->y && a->width == b->width && a->height == b->height);
}

uint8_t valid_section(const section_t* sec){
    return (!(sec->width == 0 || sec->height == 0));
}

uint8_t sections_overlapping(const section_t* a, const section_t* b){
    if (a->x + a->width <= b->x) return 0;
    if (b->x + b->width <= a->x) return 0;
    if (a->y + a->height <= b->y) return 0;
    if (b->y + b->height <= a->y) return 0;
    return 1;
}

uint8_t section_contains(const section_t* container, const section_t* containee){
    return (container->x <= containee->x &&
            container->y <= containee->y &&
            container->x + container->width >= containee->x + containee->width &&
            container->y + container->height >= containee->y + containee->height);
}

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
uint32_t n_windows = 0;

mouse_t mouse = {
    .posx = 0,
    .posy = 0,
    .icon = {
        {0xff,0xff,0xff},
        {0xff,0xff,0xff},
        {0xff,0xff,0xff}
    },
    .left_clicked = 0,
    .right_clicked = 0,
    .middle_clicked = 0
};

void add_window_to_list(window_t* win){
    // set as first -> focused window
    window_t* old_head = head;
    head = win;
    win->next = old_head;
}

void remove_window_from_list(window_t* win){
    window_t* iter = head;

    if (win == head){
        head = head->next;
        return;
    }

    while(iter->next && iter->next != win) iter = iter->next;
    if (!iter->next) return;

    iter->next = iter->next->next;

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

void add_dirty_section(section_t sec){
    if (!dirty_sections) return;
    if (dirty_section_idx >= MAX_DIRTY_SECTIONS) return;

    dirty_sections[dirty_section_idx++] = sec;
}

void handle_window_request(framebuffer_t* fb,win_man_msg_t* msg){
    window_req_t* req = (window_req_t*)msg->data;
    if (req->height > fb->height) req->height = fb->height;
    if (req->width > fb->width) req->width = fb->width;
    window_t* new_win = (window_t*)malloc(sizeof(window_t));
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
    new_win->section.x = n_windows * 50 + 1;
    new_win->section.y = n_windows * 50 + 1;
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
    if (!valid_section(sec)){
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

void handle_window_commit(framebuffer_t* fb,win_man_msg_t* msg){
    window_t* win = find_window_by_pid(msg->pid);
    if (!win) return;

    memcpy(win->comitted_buffer,win->live_buffer,win->buffer_size);
    
    add_dirty_section(win->section);
}

void write_window_to_fb(framebuffer_t* fb, window_t* win) {
    unsigned char* win_start = (unsigned char*)((uint64_t)back_buffer + win->section.y * fb->bytes_per_row + win->section.x * screen_bytespp);

    uint32_t width_copy_size = ((win->section.x + win->section.width > fb->width) ? (fb->width - win->section.x) : win->section.width) * screen_bytespp;
    uint32_t rows_to_copy = (win->section.y + win->section.height > fb->height) ? (fb->height - win->section.y) : win->section.height;

    if (!win_start || width_copy_size == 0 || rows_to_copy == 0) return;

    if (win->section.y > 0) {
        unsigned char* top_border = win_start - fb->bytes_per_row;
        memset(top_border, 0xAA, width_copy_size);
    }

    uint32_t buffer_cpy_off = 0;
    for (uint32_t i = 0; i < rows_to_copy; i++) {
        if (win->section.x > 0) {
            memset(win_start - screen_bytespp, 0xAA, screen_bytespp);
        }

        memcpy(win_start, &win->comitted_buffer[buffer_cpy_off], width_copy_size);

        if (win->section.x + win->section.width < fb->width) {
            memset(win_start + width_copy_size, 0xAA, screen_bytespp);
        }

        win_start += fb->bytes_per_row;
        buffer_cpy_off += win->section.width * screen_bytespp;
    }

    if (win->section.y + rows_to_copy < fb->height) {
        memset(win_start, 0xAA, width_copy_size);
    }
}

void clear_section(section_t sec, framebuffer_t* fb){
    if (sec.x >= fb->width || sec.y >= fb->height) return;
    
    if (sec.x + sec.width > fb->width)   sec.width  = fb->width  - sec.x;
    if (sec.y + sec.height > fb->height) sec.height = fb->height - sec.y;

    for (uint32_t j = 0; j < sec.height; j++){
        unsigned char* row_start = back_buffer + (sec.y + j) * fb->bytes_per_row + sec.x * screen_bytespp;
        memset(row_start, 0, sec.width * screen_bytespp);
    }
}


void blit_section(section_t* sec, framebuffer_t* fb) {
    for (uint32_t y = 0;y < sec->height;y++) {
        memcpy(
            fb0 + (sec->y+y)*fb->bytes_per_row + sec->x*screen_bytespp,
            back_buffer + (sec->y+y)*fb->bytes_per_row + sec->x*screen_bytespp,
            sec->width * screen_bytespp
        );
    }
}


void partial_window_copy(window_t* win, section_t* sec,framebuffer_t* fb){

    if (sec->x < win->section.x ||
        sec->y < win->section.y ||
        sec->x + sec->width  > win->section.x + win->section.width ||
        sec->y + sec->height > win->section.y + win->section.height)
            return;


    uint32_t y_offset = sec->y - win->section.y;
    uint32_t x_offset = sec->x - win->section.x;
    
    for (uint32_t y = 0; y < sec->height;y++){
        memcpy(
            back_buffer + (sec->y + y)*fb->bytes_per_row + sec->x*screen_bytespp,
            win->comitted_buffer + ((y_offset + y)*win->section.width + x_offset)*screen_bytespp,
            sec->width * screen_bytespp
        );
    }
}
section_t intersection(section_t* sec_a,section_t* sec_b){
    section_t ret_rec = {0};
    if (!sections_overlapping(sec_a,sec_b)) return ret_rec;
    ret_rec.x = max(sec_a->x,sec_b->x);
    ret_rec.y = max(sec_a->y,sec_b->y);

    uint32_t right = min(sec_a->x + sec_a->width,sec_b->x + sec_b->width);
    uint32_t bottom = min(sec_a->y + sec_a->height,sec_b->y + sec_b->height);

    ret_rec.width = right - ret_rec.x;
    ret_rec.height = bottom - ret_rec.y;
    return ret_rec;
}


section_split_t split_section(section_t* sec, section_t center_sec){
    /*
     * split sec into up to 5 subsections
     * center_sec becomes subsection 0
     * splitting vertically
    */
   section_split_t split_sec = {0};
   split_sec.sections[0] = center_sec;
    if (sec->x < center_sec.x){
        // empty space to the left
        section_t left;
        left.x = sec->x;
        left.y = sec->y;
        left.height = sec->height;
        left.width = center_sec.x - sec->x;
        split_sec.sections[1] = left;
    }
    if (sec->y < center_sec.y){
        // empty space on the top
        section_t top;
        top.x = center_sec.x;
        top.y = sec->y;
        top.height = center_sec.y - sec->y;
        top.width = center_sec.width;
        split_sec.sections[2] = top;
    }
    if (sec->x + sec->width > center_sec.x + center_sec.width){
        // empty space on the right
        section_t right;
        right.x = center_sec.x + center_sec.width;
        right.y = sec->y;
        right.height = sec->height;
        right.width = (sec->x + sec->width) - (center_sec.x + center_sec.width);
        split_sec.sections[3] = right;
    }
    if (sec->y + sec->height > center_sec.y + center_sec.height){
        // empty space on the bottom
        section_t bottom;
        bottom.x = center_sec.x;
        bottom.y = center_sec.y + center_sec.height;
        bottom.width = center_sec.width;
        bottom.height = (sec->y + sec->height) - (center_sec.y + center_sec.height);
        split_sec.sections[4] = bottom;
    }
    return split_sec;


}

uint32_t find_containing_dirty_section_idx(section_t* sec){
    for (uint32_t i = 0; i < dirty_section_idx;i++){
        if (section_contains(&dirty_sections[i],sec)){
            return i;
        }
    }
    return UINT32_MAX;
}

section_t draw_visible_window_decorations(window_t* win, section_t* visible_sec, framebuffer_t* fb){
    uint32_t sec_idx = find_containing_dirty_section_idx(visible_sec);
    if (sec_idx == UINT32_MAX) return *visible_sec;
    
    section_t* dirty_sec = &dirty_sections[sec_idx];

    if (!dirty_sec) return *visible_sec;
    if (!valid_section(dirty_sec)) return *visible_sec;
    section_t original_visible_sec = *visible_sec;

    if (visible_sec->y == win->section.y && win->section.y > 0){
        // top border
        unsigned char* top_border = back_buffer + (visible_sec->y - 1) * fb->bytes_per_row + visible_sec->x * screen_bytespp;
        uint32_t clamped_width = min(visible_sec->width, fb->width - visible_sec->x);
        memset(top_border, 0xAA, clamped_width * screen_bytespp);

        if (dirty_sec->y == visible_sec->y && !(dirty_section_expanded[sec_idx] & SECTION_EXPANDED_TOP)){
            dirty_sec->y--;
            dirty_sec->height++;
            set_section_expanded(sec_idx, SECTION_EXPANDED_TOP);
        }

        // also enlarge so that splitting does not create subsections that overwrites the border again
        visible_sec->y--;
        visible_sec->height++;

    }
    if (visible_sec->y + visible_sec->height == win->section.y + win->section.height && win->section.y + win->section.height < fb->height){
        // bottom border
        unsigned char* bottom_border = back_buffer + (visible_sec->y + visible_sec->height) * fb->bytes_per_row + visible_sec->x * screen_bytespp;
        uint32_t clamped_width = min(visible_sec->width, fb->width - visible_sec->x);
        memset(bottom_border, 0xAA, clamped_width * screen_bytespp);

        
        if (dirty_sec->y + dirty_sec->height == visible_sec->y + visible_sec->height && !(dirty_section_expanded[sec_idx] & SECTION_EXPANDED_BOTTOM)){
            dirty_sec->height++;
            set_section_expanded(sec_idx, SECTION_EXPANDED_BOTTOM);
        }
        
        visible_sec->height++;
    }
    if (visible_sec->x == win->section.x && win->section.x > 0){
        // left border
        uint32_t clamped_height = min(visible_sec->height,fb->height - visible_sec->y);
        for (uint32_t y = 0; y < clamped_height;y++){
            unsigned char* left_border = back_buffer + (visible_sec->y + y) * fb->bytes_per_row + (visible_sec->x - 1) * screen_bytespp;
            memset(left_border, 0xAA, screen_bytespp);
        }
        if (dirty_sec->x == visible_sec->x && !(dirty_section_expanded[sec_idx] & SECTION_EXPANDED_LEFT)){
            dirty_sec->x--;
            dirty_sec->width++;
            set_section_expanded(sec_idx, SECTION_EXPANDED_LEFT);
        }

        visible_sec->x--;
        visible_sec->width++;

    }
    if (visible_sec->x + visible_sec->width == win->section.x + win->section.width && win->section.x + win->section.width < fb->width){
        // right border
        uint32_t clamped_height = min(visible_sec->height,fb->height - visible_sec->y);
        for (uint32_t y = 0; y < clamped_height;y++){
            unsigned char* right_border = back_buffer + (visible_sec->y + y) * fb->bytes_per_row + (visible_sec->x + visible_sec->width) * screen_bytespp;
            memset(right_border, 0xAA, screen_bytespp);
        }

        if (dirty_sec->x + dirty_sec->width == visible_sec->x + visible_sec->width && !(dirty_section_expanded[sec_idx] & SECTION_EXPANDED_RIGHT)){
            dirty_sec->width++;
            set_section_expanded(sec_idx, SECTION_EXPANDED_RIGHT);
        }
        
        visible_sec->width++;
    }

    return original_visible_sec;

}

void update_rect(window_t* win,section_t* rect,framebuffer_t* fb){
    
    if (!win){
        clear_section(*rect,fb); // normally draw background here
        return;
    }
    
    section_t visible_intersection = intersection(&win->section,rect);
    if (valid_section(&visible_intersection)){
        // original intersec needed for partial window copy bc visible_intersection may be modified for borders etc
        section_t original_intersec = draw_visible_window_decorations(win, &visible_intersection, fb);

        section_split_t split_sec = split_section(rect,visible_intersection);
        partial_window_copy(win,&original_intersec,fb);

        for (uint32_t i = 1; i < 5;i++){
            if (valid_section(&split_sec.sections[i]) ){
                update_rect(win->next,&split_sec.sections[i],fb);
            }
        }
    }else{
        update_rect(win->next,rect,fb);
    }
}


uint8_t sections_overlapping_large(section_t* a,section_t* b){
    // checks if the sections overlap enough to merge them 
    if (!sections_overlapping(a,b)) return 0;
    section_t intersec = intersection(a,b);
    uint32_t intersec_area = intersec.width * intersec.height;
    section_t a_copy = *a;
    enlarge_section(&a_copy,b->x,b->y,b->width,b->height);

    // area which gets added by merging the two rects
    uint32_t extra_area = (a_copy.width * a_copy.height) - (b->height * b->width) - (a->height * a->width) + intersec_area;

    return intersec_area > extra_area;
}

void merge_dirty_sections(){
    // only merges identical sections right now (are created when moving)
    for (uint32_t i = 0; i < dirty_section_idx;i++){
        if (!valid_section(&dirty_sections[i])) continue;
        for (uint32_t j = 0; j < dirty_section_idx;j++){
            if (i == j) continue;
            if (!valid_section(&dirty_sections[j])) continue;
            if (sections_overlapping_large(&dirty_sections[i],&dirty_sections[j])){
                enlarge_section(
                    &dirty_sections[i],
                    dirty_sections[j].x,
                    dirty_sections[j].y,
                    dirty_sections[j].width,
                    dirty_sections[j].height
                );

                dirty_sections[j].height = 0;
                dirty_sections[j].width = 0;
            }
        }
    }

    // remove invalid sections
    uint32_t dst = 0;
    for (uint32_t i = 0; i < dirty_section_idx;i++){
        if (valid_section(&dirty_sections[i])){
            if (dst != i) dirty_sections[dst] = dirty_sections[i];
            dst++;
        }
    }
    dirty_section_idx = dst;
}

void clamp_dirty_sections(framebuffer_t* fb){
    for (uint32_t i = 0; i < dirty_section_idx;i++){
        if (dirty_sections[i].x >= fb->width || dirty_sections[i].y >= fb->height){
            dirty_sections[i].width = 0;
            dirty_sections[i].height = 0;
            continue;
        }
        dirty_sections[i].width = min(dirty_sections[i].width, fb->width - dirty_sections[i].x);
        dirty_sections[i].height = min(dirty_sections[i].height, fb->height - dirty_sections[i].y);
    }
}

// draw a 3x3 square for the mouse pointer
void draw_mouse(framebuffer_t* fb){
    unsigned char* mouse_start = (unsigned char*)((uint64_t)back_buffer + mouse.posy * fb->bytes_per_row + mouse.posx * screen_bytespp);
    // draw mouse icon and save the pixels that are being overridden
    for (uint16_t x = 0; x < sizeof(mouse.icon[0]); x++){
        for (uint16_t y = 0; y < sizeof(mouse.icon) / sizeof(mouse.icon[0]); y++){
            unsigned char* pixel = mouse_start + y * fb->bytes_per_row + x * screen_bytespp;
            uint32_t row = (pixel - back_buffer) / fb->bytes_per_row;
            uint8_t* start = back_buffer;
            uint8_t* end = back_buffer + fb->size;

            if (pixel < start || pixel + screen_bytespp > end || row != mouse.posy + y)
                continue; // sanity check to avoid writing out of bounds
            
            memset(pixel,mouse.icon[y][x],screen_bytespp);
        }
    }
}

void composite_windows(framebuffer_t* fb){
    if (!head) return;
    merge_dirty_sections();
    clamp_dirty_sections(fb);
    
    // for every dirty rect update and blit them
    for (uint32_t i = 0; i < dirty_section_idx;i++){
        section_t* sec = &dirty_sections[i];
        if (!valid_section(sec)) continue;
        update_rect(head,sec,fb);
    }
    for (uint32_t i = 0;i < dirty_section_idx;i++){
        // doing this in a seperate loop so that there is less screen tearing from the extra update time that update_rect takes
        section_t* sec = &dirty_sections[i];
        if (!valid_section(sec)) continue;
        blit_section(sec,fb);
    }
    draw_mouse(fb);
    section_t mouse_sec = {mouse.posx,mouse.posy,3,3};
    blit_section(&mouse_sec,fb);
    memset(dirty_section_expanded,0,sizeof(dirty_section_expanded));
    dirty_section_idx = 0;
    
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
    
    remove_window_from_list(win);

    // clean it up
    if (win->section.x > 0) win->section.x--;
    if (win->section.y > 0) win->section.y--;
    win->section.width  += 2;
    win->section.height += 2;
    add_dirty_section(win->section);
    free(win);
    win = 0;
    free(pid_str);

}

window_t* get_mouse_top_window(){
    window_t* iter = head;
    window_t* win = 0;
    while(iter){
        if (mouse.posx >= iter->section.x &&
            mouse.posx <= iter->section.x + iter->section.width &&
            mouse.posy >= iter->section.y &&
            mouse.posy <= iter->section.y + iter->section.height 
        ){
            return iter; // is highest window since we go from highest -> lowest
        }
        iter = iter->next;
    }

    return 0; 
}

void adjust_window_order(window_t* new_top){
    remove_window_from_list(new_top);
    add_window_to_list(new_top); // inserts at start
}


void move_window(window_t* win,framebuffer_t* fb,int16_t relx, int16_t rely){
    add_dirty_section(win->section); // add dirty even only clicked so that it is highlighted
    if (relx == 0 && rely == 0) return;
    int32_t new_x = (int32_t)win->section.x + relx;
    int32_t new_y = (int32_t)win->section.y - rely;

    if (new_x < 0) new_x = 0;
    if (new_x >= (int32_t)fb->width) new_x = fb->width - 1;
    if (new_y < 0) new_y = 0;
    if (new_y >= (int32_t)fb->height) new_y = fb->height - 1;

    section_t dirty_sec = win->section;

    win->section.x = (uint32_t)new_x;
    win->section.y = (uint32_t)new_y;

    enlarge_section(&dirty_sec,win->section.x,win->section.y,win->section.width,win->section.height);
    if (dirty_sec.x > 0) dirty_sec.x--; // add 1 pixel border to avoid artifacts from window decorations
    if (dirty_sec.y > 0) dirty_sec.y--;
    dirty_sec.width += 2;
    dirty_sec.height += 2;
    add_dirty_section(dirty_sec);
}

void update_mouse(framebuffer_t* fb,int mouse_fd){
    mouse_packet_t buffer[MAX_MOUSE_PACKETS];
    int bytes_read = read(mouse_fd,(const char*)buffer,sizeof(buffer));
    if (bytes_read <= 0) return;

    getpid(); // if I remove this useless syscall I get a page fault, I dont know why.. please help, temporary support pillar here

    uint32_t n_packets = bytes_read / sizeof(mouse_packet_t);
    for (uint32_t i = 0; i < n_packets;i++){

        int16_t rel_x = buffer[i].relx;
        int16_t rel_y = buffer[i].rely;  
        
        if (buffer[i].btns & MOUSE_BTN_LEFT_MASK) {
            if (!mouse.tried_window){
                // only tries once so that a clicked mouse moving over a window does not start to move that window
                mouse.dragging_window = get_mouse_top_window();
                if (mouse.dragging_window) add_dirty_section(mouse.dragging_window->section);
                mouse.tried_window = 1;
            }

            if (mouse.dragging_window) {
                adjust_window_order(mouse.dragging_window);
                if (mouse.left_clicked) {
                    move_window(mouse.dragging_window, fb, rel_x, rel_y);
            
                } else {
                    mouse.left_clicked = 1;
                }
            }
        } else {
            mouse.left_clicked = 0;
            mouse.tried_window = 0;
            mouse.dragging_window = 0;
        }

        int32_t new_x = (int32_t)mouse.posx + rel_x;
        int32_t new_y = (int32_t)mouse.posy - rel_y;
        section_t old_mouse = {mouse.posx, mouse.posy, 3, 3};
        if (new_x < 0) new_x = 0;
        if (new_x >= (int32_t)fb->width) new_x = fb->width - 1;
        if (new_y < 0) new_y = 0;
        if (new_y >= (int32_t)fb->height) new_y = fb->height - 1;
        
        mouse.posx = (uint32_t)new_x;
        mouse.posy = (uint32_t)new_y;
        section_t new_mouse = {mouse.posx, mouse.posy, 3, 3};

        
        add_dirty_section(old_mouse);
        add_dirty_section(new_mouse);
        
    }
}

__attribute__((section(".text.start")))
void main(){
    int fb0_fd   = open("dev/fb0",FILE_FLAG_NONE);
    int kb0_fd   = open("dev/kb0",FILE_FLAG_NONE);
    int mouse_fd = open("dev/mouse",FILE_FLAG_NONE);
    wm_to_k_fd   = open("tmp/wm_to_k.tmp",FILE_FLAG_WRITE);
    k_to_wm_fd   = open("tmp/k_to_wm.tmp",FILE_FLAG_READ);

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

    
    framebuffer_t fb;
    if (ioctl(fb0_fd,DEV_FB0_GET_METADATA,&fb) < 0) exit(3);
    screen_bytes_per_row = fb.bytes_per_row;
    screen_bytespp = fb.bpp / 8;
    
    dirty_sections = (section_t*)mmap((MAX_DIRTY_SECTIONS * sizeof(section_t)),PROT_READ | PROT_WRITE,MAP_ANON,0,0);
    memset(dirty_sections,0,MAX_DIRTY_SECTIONS * sizeof(section_t));
    fb0 = (unsigned char*)mmap(fb.size, PROT_READ | PROT_WRITE, MAP_SHARED, fb0_fd,0);
    back_buffer = (unsigned char*)mmap(fb.size,PROT_READ | PROT_WRITE,MAP_ANON,0,0);
    memset(back_buffer,0,fb.size);
    close(fb0_fd);
    if (!fb0) exit(4);
    if (!dirty_sections) exit(5);
    if (!back_buffer) exit(6);
    mouse.posx = fb.width / 2;
    mouse.posy = fb.height / 2;
    
    unsigned char buffer[256];
    memset(buffer,0,sizeof(buffer));
    
    uint32_t n_input = 0;
    uint32_t n_kmsg = 0;
    while(1){
        n_input = read(kb0_fd,buffer,sizeof(buffer));
        if (head && n_input > 0){
            write(head->kb_pipe_fd,buffer,n_input);
        }

        memset(buffer,0,sizeof(buffer));
        
        n_kmsg = read(k_to_wm_fd,buffer,sizeof(buffer));
        uint32_t bytes_read = 0;
        
        while (bytes_read < n_kmsg){
            
            win_man_msg_t* msg = (win_man_msg_t*)&buffer[bytes_read];
            switch (msg->cmd)
            {
                case DEV_WM_REQUEST_WINDOW:{
                    handle_window_request(&fb,msg);
                    break;
                }
                case DEV_WM_COMMIT_WINDOW: {
                    handle_window_commit(&fb,msg);
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

        update_mouse(&fb,mouse_fd);
        composite_windows(&fb);
    }

    exit(EXIT_SUCCESS);
}
