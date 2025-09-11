
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
#define MOUSE_GET_DEV_ID 0xf2
#define MOUSE_SET_SAMPLE_RATE 0xf3
#define MOUSE_ENABLE_DATA_REPORT 0xf4
#define MOUSE_DISABLE_DATA_REPORT 0xf5
#define MOUSE_SET_DEFAULTS 0xf6
#define MOUSE_RESEND 0xfe
#define MOUSE_RESET 0xff

#define MOUSE_ACK 0xfa

#define MOUSE_SAMPLE_RATE 100
void init_mouse();

void handle_mouse_interrupt();

#endif