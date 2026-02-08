#include "mouse.h"
#include "../ps2_controller.h"
#include "../../../io/io.h"
#include "../../../io/log.h"
#include "../../../tables/interrupts.h"
#include "../../../ACPI/acpi.h"
#include <stdint.h>

static uint8_t mouse_cycle = 0;
static uint8_t mouse_status;

static int16_t mouse_x;
static int16_t mouse_y;

void init_mouse(ps2_ports_t port){
    ps2_port_enable(port);
    if (!(ps2_port_write(port,PORT_SET_DEFAULTS) && ps2_port_read(1) == ACK)) error("Failed setting mouse defaults");
    if (!(ps2_port_write(port,PORT_SET_SAMPLE_RATE) && ps2_port_read(1) == ACK)) error("Failed to request mouse sample rate");
    if (!(ps2_port_write(port,MOUSE_SAMPLE_RATE) && ps2_port_read(1) == ACK)) error("Failed to set mouse sample rate");
    if (!(ps2_port_write(port,PORT_ENABLE_DATA_REPORT) && ps2_port_read(1) == ACK)) error("Failed to enable mouse data reporting");

    uint8_t irq = ioapic_redirect_irq(12);
    register_irq(irq,handle_mouse_interrupt);
    log("Initialized the PS/2 mouse");
}

void handle_mouse_interrupt(interrupt_stack_frame_t* frame){
    uint8_t data = ps2_port_read(0);

    // packet looks like:
    // byte:     7              6           5           4          3          2                    1                0
    // byte 0: y-overflow | x-overflow | y-sign bit | x-sign bit | 1 | middle click down | right click down | left click down
    // byte 1: x-axis movement val
    // byte 2: y-axis movement val

    switch (mouse_cycle)
    {
    case 0:
        mouse_status = data;
        break;
    case 1: 
        mouse_x = data;
        break;
    case 2:
        mouse_y = data;
        break;
    }

    mouse_cycle = (mouse_cycle + 1) % 3;

    if (mouse_cycle != 0) return; // there is more to parse

    if(mouse_status & 0x80 || mouse_status & 0x40) return;

    uint8_t left_clicked = mouse_status & 0x1;
    uint8_t right_clicked = mouse_status & 0x2;
    uint8_t middle_clicked = mouse_status & 0x4;
    
    if (left_clicked) log("Leftclick");
    if (right_clicked) log("Rightclick");
    if (middle_clicked) log("Middleclick");
    
    int16_t rel_x = mouse_x - ((mouse_status << 0x4) & 0x100);
    int16_t rel_y = mouse_y - ((mouse_status << 0x3) & 0x100);
}
