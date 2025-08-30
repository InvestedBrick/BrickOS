
#ifndef INCLUDE_DEVICE_COMMANDS_H
#define INCLUDE_DEVICE_COMMANDS_H
#include <stdint.h>

// these defines / structs are accessible for the userspace
#define DEV_FB0_GET_METADATA 0x01

#define DEV_WM_REQUEST_WINDOW 0x01
#define DEV_WM_REQUEST_WINDOW_CREATION_ANSWER 0x02
#define DEV_WM_COMMIT_WINDOW 0x03

#define DEV_WM_ANS_TYPE_WIN_CREATION 0x04
#define WINDOW_REQ_FLAG_DECORATION 0x02
typedef struct {
    uint32_t phys_addr;
    uint32_t width;
    uint32_t height;
    uint32_t bpp;
    uint32_t bytes_per_row;
    uint32_t size;
}framebuffer_t;

typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t flags;
}window_req_t;

typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t filename_len;
    unsigned char filename[];
}window_creation_wm_answer_t;


// this is a kernel-wm only struct, do not use this to recieve your window creation answers
// I am too tired to think of actually good struct names rn
typedef struct {
    uint32_t answer_type;
    uint32_t pid; // the pid of the recieving user proc
    uint32_t width; // there may be some adjustments
    uint32_t height;
    uint32_t filename_len;
    unsigned char filename[];
}window_creation_ans_t;

#endif