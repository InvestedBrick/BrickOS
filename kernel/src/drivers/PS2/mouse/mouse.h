
#ifndef INCLUDE_MOUSE_H
#define INCLUDE_MOUSE_H

#define MOUSE_SCALING_1_1 0xe6
#define MOUSE_SCALING_2_1 0xe7
#define MOUSE_SET_RES 0xe8
#define MOUSE_STATUS_REQ 0xe9
#define MOUSE_SET_STREAM_MODE 0xea
#define MOUSE_READ_DATA 0xeb
#define MOUSE_RESET_WRAP 0xec
#define MOUSE_SET_WRAP 0xee
#define MOUSE_SET_REMOTE_MODE 0xf0

#define MOUSE_SAMPLE_RATE 80 

#define MOUSE_BUFFER_SIZE 256
#include "../ps2_controller.h"
#include "../../../tables/interrupts.h"
#include "../../../filesystem/vfs/vfs.h"

void init_mouse(ps2_ports_t port);

extern vfs_handles_t mouse_ops;

void handle_mouse_interrupt(interrupt_stack_frame_t* frame);

#endif