#include "mouse.h"
#include "../ps2_controller.h"
#include "../../../io/io.h"
#include "../../../io/log.h"
#include "../../../tables/interrupts.h"
#include "../../../ACPI/acpi.h"
#include "../../../filesystem/vfs/vfs.h"
#include "../../../filesystem/devices/device_defines.h"
#include <stdint.h>

static uint8_t mouse_cycle = 0;
static uint8_t mouse_status;

static int16_t mouse_x;
static int16_t mouse_y;


mouse_packet_t packets[MOUSE_BUFFER_SIZE] = {0};
uint32_t mouse_head = 0;
uint32_t mouse_tail = 0;

void mouse_buffer_push(mouse_packet_t packet){
    uint32_t next = (mouse_head + 1) % MOUSE_BUFFER_SIZE;
    if (next != mouse_tail){
        packets[mouse_head] = packet;
        mouse_head = next;
    }
}

int mouse_buffer_pop(mouse_packet_t* packet){
    if (mouse_head == mouse_tail) return 0;
    *packet = packets[mouse_tail];
    mouse_tail = (mouse_tail + 1) % MOUSE_BUFFER_SIZE;
    return 1;
}


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

int mouse_read(generic_file_t* f, unsigned char* buffer, uint32_t size){
    if (size > MOUSE_BUFFER_SIZE * sizeof(mouse_packet_t)) size = MOUSE_BUFFER_SIZE * sizeof(mouse_packet_t);
    
    size = (size / sizeof(mouse_packet_t)) * sizeof(mouse_packet_t); // align down to mouse packet size
    
    int bytes_read = 0;
    while (bytes_read < size){
        if(!mouse_buffer_pop((mouse_packet_t*)&buffer[bytes_read])){
            break;
        }
        bytes_read += sizeof(mouse_packet_t);
    }
    return bytes_read;
}

vfs_handles_t mouse_ops = {
    .open = 0,
    .close = 0,
    .read = mouse_read,
    .write = 0,
    .seek = 0,
    .ioctl = 0,
};

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
    
    int16_t rel_x = mouse_x;
    if (mouse_status & 0x10) {  // X sign bit
        rel_x |= 0xFF00;  // sign extend to negative
    }

    int16_t rel_y = mouse_y;
    if (mouse_status & 0x20) {  // Y sign bit
        rel_y |= 0xFF00;  // sign extend to negative
    }

    mouse_packet_t packet;
    packet.relx = rel_x;
    packet.rely = rel_y;
    packet.btns = 0;
    if (left_clicked) packet.btns |= (1 << 0);
    if (middle_clicked) packet.btns |= (1 << 1);
    if (right_clicked) packet.btns |= (1 << 2);
    mouse_buffer_push(packet);
}
