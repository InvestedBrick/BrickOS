#include "mouse.h"
#include "../ps2_controller.h"
#include "../../../io/io.h"
#include "../../../io/log.h"
#include <stdint.h>


void mouse_wait(uint8_t w_type) {
    uint32_t time_out = MOUSE_WAIT_TIMEOUT;
    if (w_type == MOUSE_WAIT_READ) {
        
        while (--time_out) 
            if (inb(PS2_STATUS_PORT) & 0x1) return;
    } else {
        
        while (--time_out)  
            if(!(inb(PS2_STATUS_PORT) & 0x2)) return;
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

void mouse_check_ack(){
    if (mouse_read() != MOUSE_ACK) warn("MOUSE FAILED TO ACK");
}

void init_mouse(){
    mouse_wait(MOUSE_WAIT_WRITE);
    outb(PS2_CMD_PORT,ENABLE_SECOND_PORT);

    mouse_wait(MOUSE_WAIT_WRITE);
    outb(PS2_CMD_PORT,READ_CONTROLLER_CONFIG);
    mouse_wait(MOUSE_WAIT_READ);
    uint8_t config = (inb(PS2_DATA_PORT) | 0x2);

    mouse_wait(MOUSE_WAIT_WRITE);
    outb(PS2_CMD_PORT,WRITE_CONTROLLER_CONFIG);

    mouse_wait(MOUSE_WAIT_WRITE);
    outb(PS2_DATA_PORT,config);

    mouse_write(MOUSE_SET_DEFAULTS);    
    mouse_check_ack();

    //mouse_write(MOUSE_SET_SAMPLE_RATE);
    //mouse_check_ack();
    //mouse_write(MOUSE_SAMPLE_RATE);
    //mouse_check_ack();

    mouse_write(MOUSE_ENABLE_DATA_REPORT);
    mouse_check_ack();
    ps2_flush_output();

}

void mouse_sanity_check(){
    uint8_t check_passed = 1;

    ps2_flush_output();
    mouse_write(MOUSE_STATUS_REQ);
    mouse_read();
    if (!(mouse_read() & 0x20)) {warn("MOUSE NOT ENABLED"); check_passed = 0;} // status, should have bit 5 set
    log_uint((uint32_t)mouse_read()); // resolution
    log_uint((uint32_t)mouse_read()); // sample rate
    //if (mouse_read() != MOUSE_SAMPLE_RATE) {warn("MOUSE SAMPLE RATE INCORRECT");check_passed = 0;}

    // read controller config byte
    mouse_wait(MOUSE_WAIT_WRITE);
    outb(PS2_CMD_PORT,READ_CONTROLLER_CONFIG);
    mouse_wait(MOUSE_WAIT_READ);
    uint8_t config = mouse_read();
    if (!(config & 0x2)){ warn("MOUSE INTERRUPTS DISABLED");check_passed = 0;}

    // PIC IMR check
    mouse_wait(MOUSE_WAIT_WRITE);
    uint8_t pic_imr = inb(0x21);
    if (pic_imr & 0x20){warn("MOUSE INTERRUPTS MASKED BY PIC");check_passed = 0;}   

    uint8_t pic2_imr = inb(0xa1);
    if (pic2_imr & (1 << 4)) { // IRQ12 = bit 4 on PIC2
        warn("MOUSE INTERRUPTS MASKED BY PIC2");
        check_passed = 0;
    }

    if (check_passed) log("Mouse sanity check passed");
    else error("Mouse sanity check failed");
}


void handle_mouse_interrupt(){

    static uint8_t mouse_cycle = 0;
    static uint8_t mouse_status;

    static int16_t mouse_x;
    static int16_t mouse_y;

    if (!(inb(PS2_STATUS_PORT) & 0x20)){ // no bytes for us
        return;
    }

    // packet looks like:
    // byte:     7              6           5           4          3          2                    1                0
    // byte 0: y-overflow | x-overflow | y-sign bit | x-sign bit | 1 | middle click down | right click down | left click down
    // byte 1: x-axis movement val
    // byte 2: y-axis movement val

    switch (mouse_cycle)
    {
    case 0:
        mouse_status = inb(PS2_DATA_PORT);
        break;
    case 1: 
        mouse_x = inb(PS2_DATA_PORT);
        break;
    case 2:
        mouse_y = inb(PS2_DATA_PORT);
        break;
    }

    mouse_cycle = (mouse_cycle + 1) % 3;

    if (mouse_cycle != 0) return; // there is more to parse

    uint8_t left_clicked = mouse_status & 0x1;
    uint8_t right_clicked = mouse_status & 0x2;
    uint8_t middle_clicked = mouse_status & 0x4;
    
    if (left_clicked) log("Leftclick");
    if (right_clicked) log("Rightclick");
    if (middle_clicked) log("Middleclick");
    
    if (mouse_status & (1 << 4)) mouse_x -= 0x100;
    if (mouse_status & (1 << 5)) mouse_x -= 0x100;
        
}
