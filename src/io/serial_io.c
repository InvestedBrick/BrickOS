#include "io.h"
#include "../tables/interrupts.h"
void serial_configure_baud_rate(uint16_t com, uint16_t divisor){
    outb(SERIAL_LINE_COMMAND_PORT(com), SERIAL_LINE_ENABLE_DLAB);
    outb(SERIAL_DATA_PORT(com), (divisor >> 8) & 0x00ff);
    outb(SERIAL_DATA_PORT(com), divisor & 0x00ff);
}

void serial_configure_line(uint16_t com){
    /* Bit:     | 7 | 6 | 5 4 3 | 2 | 1 0 |
     * Content: | d | b | prty  | s | dl  |
     * Value:   | 0 | 0 | 0 0 0 | 0 | 1 1 | = 0x03
     */
    outb(SERIAL_LINE_COMMAND_PORT(com), 0x3);
}

void serial_configure_buffer(uint16_t com){
    /**
     * Bit:     | 7 6 | 5  | 4 | 3   | 2   | 1   | 0 |
     * Content: | lvl | bs | r | dma | clt | clr | e |
     * Value:   | 1 1 | 0  | 0 | 0   | 1   | 1   | 1 | = 0xc7 
     */
    outb(SERIAL_FIFO_COMMAND_PORT(com),0xc7);
}

void serial_configure_modem(uint16_t com) {
    /**
     * 
     * Bit:     | 7 | 6 | 5  | 4  | 3   | 2   | 1   | 0   |
     * Content: | r | r | af | lb | ao2 | ao1 | rts | dtr |
     * Value:   | 0 | 0 | 0  | 0  | 0   | 0   | 1    |1   | = 0x03
     */
    outb(SERIAL_MODEM_COMMAND_PORT(com),0x03);
}

int serial_is_transmit_fifo_empty(uint32_t com){
    return (inb(SERIAL_LINE_STATUS_PORT(com)) & 0x20);
}

void serial_write(const unsigned char* data, uint16_t com){
    uint8_t old_int = get_interrupt_status();
    disable_interrupts();
    for(uint32_t i = 0; data[i] != '\0';++i){
        // wait until transmit FIFO is empty
        while(!serial_is_transmit_fifo_empty(com));
        outb(SERIAL_DATA_PORT(com),data[i]);
    }
    set_interrupt_status(old_int);
}