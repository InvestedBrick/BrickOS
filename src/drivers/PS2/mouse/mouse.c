#include "mouse.h"
#include "../ps2_controller.h"
#include "../../../io/io.h"
#include "../../../io/log.h"
#include <stdint.h>


void mouse_wait(uint8_t w_type) {
    uint32_t time_out = MOUSE_WAIT_TIMEOUT;
    if (w_type == MOUSE_WAIT_READ) {
        
        while (--time_out && !(inb(PS2_STATUS_PORT) & 0x1)) { }
    } else {
        
        while (--time_out && (inb(PS2_STATUS_PORT) & 0x2)) { }
    }
}


void mouse_write(uint8_t byte){
    mouse_wait(MOUSE_WAIT_WRITE);
    outb(PS2_CMD_PORT,PS2_WRITE_NEXT_BYTE_TO_SECOND_PORT);
    mouse_wait(MOUSE_WAIT_WRITE);
    outb(PS2_DATA_PORT,byte);
}

uint8_t mouse_read(){
    mouse_wait(MOUSE_WAIT_READ);
    uint8_t byte = inb(PS2_DATA_PORT);
    return byte;
}

void await_ack(){
    uint8_t ack;
    do {
        ack = mouse_read();
    } while (ack != MOUSE_ACK);
}

void init_mouse(){

    ps2_flush_output();
    mouse_wait(MOUSE_WAIT_WRITE);
    outb(PS2_CMD_PORT,READ_CONTROLLER_CONFIG);
    mouse_wait(MOUSE_WAIT_READ);
    uint8_t config = mouse_read() | 0x2;

    mouse_wait(MOUSE_WAIT_WRITE);
    outb(PS2_CMD_PORT,WRITE_CONTROLLER_CONFIG);

    mouse_wait(MOUSE_WAIT_WRITE);
    outb(PS2_DATA_PORT,config);

    //mouse_write(MOUSE_SET_SAMPLE_RATE);
    //await_ack();
    //mouse_write(MOUSE_SAMPLE_RATE);
    //await_ack();

    mouse_write(MOUSE_ENABLE_DATA_REPORT);
    await_ack();

}

static int packet[3];
static int mouse_cycle = 0;

void handle_mouse_interrupt(){


    uint8_t byte = inb(PS2_DATA_PORT);
    // packet looks like:
    // byte:     7              6           5           4          3          2                    1                0
    // byte 0: y-overflow | x-overflow | y-sign bit | x-sign bit | 1 | middle click down | right click down | left click down
    // byte 1: x-axis movement val
    // byte 2: y-axis movement val
    packet[mouse_cycle++] = byte;

    mouse_cycle %= 3;
    
    if (mouse_cycle == 0){
        uint8_t y_of = packet[0] & 0x80;
        uint8_t x_of = packet[0] & 0x40;


        uint8_t left_clicked = packet[0] & 0x1;
        uint8_t right_clicked = packet[0] & 0x2;
        uint8_t middle_clicked = packet[0] & 0x4;
        
        if (left_clicked) log("Leftclick");
        if (right_clicked) log("Rightclick");
        if (middle_clicked) log("Middleclick");
        
        int32_t rel_x = packet[1];
        int32_t rel_y = packet[2];
        if (packet[0] & (1 << 4)) rel_x -= 0x100;
        if (packet[0] & (1 << 5)) rel_y -= 0x100;

        
    }
}
