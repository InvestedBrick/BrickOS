#include "ps2_controller.h"
#include "../../io/log.h"
#include "../../io/io.h"

#include <stdint.h>

void await_read_signal(){
    while(!(inb(PS2_STATUS_PORT) & 0x1));
}

void await_write_signal(){
    while(inb(PS2_STATUS_PORT) & 0x2);
}

void ps2_flush_output() {
    while (inb(PS2_STATUS_PORT) & 0x1) {
        inb(PS2_DATA_PORT); 
    }
}


void init_and_test_I8042_controller(){
    
    await_write_signal();
    outb(PS2_CMD_PORT, DISABLE_FIRST_PORT);
    await_write_signal();
    outb(PS2_CMD_PORT, DISABLE_SECOND_PORT);
    ps2_flush_output();

    // update the controller config
    await_write_signal();
    outb(PS2_CMD_PORT,READ_CONTROLLER_CONFIG);
    await_read_signal();
    uint8_t config = inb(PS2_DATA_PORT);
    config |= 0x3; // enable both ps/2 port interrupts
    
    await_write_signal();
    outb(PS2_CMD_PORT,WRITE_CONTROLLER_CONFIG);
    await_write_signal();
    outb(PS2_DATA_PORT,config);

    // some tests to look more professional
    uint8_t result;

    await_write_signal();
    outb(PS2_CMD_PORT,TEST_PS2_CONTROLLER);
    await_read_signal();
    result = inb(PS2_DATA_PORT);
    if (result != PS2_CONTROLLER_TEST_PASSED) error("PS/2 controller failed test");

    await_write_signal();
    outb(PS2_CMD_PORT,TEST_FIRST_PORT);
    await_read_signal();
    result = inb(PS2_DATA_PORT);
    if (result != PORT_TEST_PASSED) error("PS/2 port 1 failed test");

    await_write_signal();
    outb(PS2_CMD_PORT,TEST_SECOND_PORT);
    await_read_signal();
    result = inb(PS2_DATA_PORT);
    if (result != PORT_TEST_PASSED) error("PS/2 port 2 failed test");

    await_write_signal();
    outb(PS2_CMD_PORT,ENABLE_FIRST_PORT);
    await_write_signal();
    outb(PS2_CMD_PORT,ENABLE_SECOND_PORT);

    ps2_flush_output();

}