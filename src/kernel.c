#include "io.h"
#include "log.h"
#include "screen.h"

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

    log("Hello Log");
    warn("Watch out!");
    error("FAILED");
    return;
}