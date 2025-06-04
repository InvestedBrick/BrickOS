#include "screen.h"


void kmain(void)
{
    const char* msg = "Hello BrickOS!";
    clear_screen();
    write_string(msg,15);

    return;
}