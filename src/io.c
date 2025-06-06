#include "io.h"

void serial_configure_baud_rate(unsigned short com, unsigned short divisor){
    outb(SERIAL_LINE_COMMAND_PORT(com), SERIAL_LINE_ENABLE_DLAB);
    outb(SERIAL_DATA_PORT(com), (divisor >> 8) & 0x00ff);
    outb(SERIAL_DATA_PORT(com), divisor & 0x00ff);
    ' test'
}