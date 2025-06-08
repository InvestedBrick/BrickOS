#include "io/io.h"
#include "io/log.h"
#include "screen.h"
#include "tables/gdt.h"
#include "tables/interrupts.h"
#include "drivers/keyboard/keyboard.h"
void kmain(void)
{
    const char* msg = "Hello BrickOS!";
    clear_screen();
    write_string(msg,15);

    // Serial port setup
    serial_configure_baud_rate(SERIAL_COM1_BASE,3);
    serial_configure_line(SERIAL_COM1_BASE);
    serial_configure_buffer(SERIAL_COM1_BASE);
    serial_configure_modem(SERIAL_COM1_BASE);

    log("Set up serial port");
    // Set up global descriptor table
    init_gdt();

    //Set up Interrupt descriptor table
    init_idt();

    //Initialize keyboard
    init_keyboard();

    //get input
    while(1){
        while(!kb_buffer_is_empty())
            handle_screen_input();
        __asm__ volatile ("hlt");
    }
    return;
}