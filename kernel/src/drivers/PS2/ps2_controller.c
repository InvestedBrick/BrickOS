#include "ps2_controller.h"
#include "../../io/log.h"
#include "../../io/io.h"
#include "keyboard/keyboard.h"
#include "mouse/mouse.h"
#include <stdint.h>

// heavily inspired by https://github.com/elysium-os/cronus/blob/9bee36ab5e24cbee548deebfb477027dd9db51e0/kernel/src/arch/amd64/drivers/ps2.c#L181

static uint8_t g_port_one_operational;
static uint8_t g_port_two_operational;

static uint8_t g_port_ids[2][2];

uint8_t wait_read_signal(){
    int timeout = TIMEOUT;
    while (--timeout) if (inb(PS2_STATUS_PORT) & 0x1) return 1;
    return 0;
}

uint8_t wait_write_signal(){
    int timeout = TIMEOUT;
    while(--timeout) if (!(inb(PS2_STATUS_PORT) & 0x2)) return 1;
    return 0;
}

void ps2_flush_output() {
    while (inb(PS2_STATUS_PORT) & 0x1) {
        inb(PS2_DATA_PORT); 
    }
}

void ps2_write(uint16_t port, uint8_t value){
    if (wait_write_signal()) return outb(port,value);
    error("PS/2 controller not responding to write");
}

uint8_t ps2_read(uint16_t port){
    if (wait_read_signal()) return inb(port);
    error("PS/2 controller not responding to read");
    return 0;
}

uint8_t ps2_port_write(ps2_ports_t port, uint8_t value){
    if (port == PS2_PORT_TWO){
        if (!wait_write_signal()) return 0;
        outb(PS2_CMD_PORT,PS2_WRITE_NEXT_BYTE_TO_SECOND_PORT);
    }
    if (!wait_write_signal()) return 0;
    outb(PS2_DATA_PORT,value);
    return 1;
}

uint8_t ps2_port_read(uint8_t wait){
    if (!wait || (wait && wait_read_signal())) return inb(PS2_DATA_PORT);
    error("PS/2 controller not responding to read");
    return 0;
}


void ps2_port_enable(ps2_ports_t port){
    ps2_write(PS2_CMD_PORT, port == PS2_PORT_ONE ? ENABLE_FIRST_PORT : ENABLE_SECOND_PORT );
}

void ps2_port_disable(ps2_ports_t port){
    ps2_write(PS2_CMD_PORT, port == PS2_PORT_ONE ? DISABLE_FIRST_PORT : DISABLE_SECOND_PORT );
}

uint8_t ps2_check_ack(){
    if (!wait_read_signal()) return 0;
    return inb(PS2_DATA_PORT) == ACK;
}

uint8_t ps2_port_init(ps2_ports_t port){
    uint8_t ack;

    if (!ps2_port_write(port,PS2_PORT_RESET)) return 0;
    if (!ps2_check_ack()) return 0;
    if (!wait_write_signal()) return 0;
    uint8_t reset = inb(PS2_DATA_PORT);
    inb(PS2_DATA_PORT);
    if (reset != 0xaa) return 0;

    if (!ps2_port_write(port,PORT_DISABLE_DATA_REPORT)) return 0;
    if (!ps2_check_ack()) return 0;

    if (!ps2_port_write(port,PORT_GET_DEV_ID)) return 0;
    if (!ps2_check_ack()) return 0;
    if (!wait_write_signal()) return 0;
    g_port_ids[port][0] = inb(PS2_DATA_PORT);
    if (wait_write_signal()) g_port_ids[port][1] = inb(PS2_DATA_PORT);

    if (!ps2_port_write(port,PORT_ENABLE_DATA_REPORT)) return 0;
    if (!ps2_check_ack()) return 0;

    return 1;
    
}

void ps2_port_init_driver(ps2_ports_t port){
    if(g_port_ids[port][0] == PS2_MOUSE_ID1 && g_port_ids[port][1] == PS2_MOUSE_ID2) {
        init_mouse(port);
    }
    if(g_port_ids[port][0] == PS2_KB_ID1 && g_port_ids[port][1] == PS2_KB_ID2) {
        if(port == PS2_PORT_ONE) {
            ps2_write(PS2_CMD_PORT, 0x20);
            uint8_t configuration_byte = ps2_read(PS2_DATA_PORT);
            configuration_byte |= PS2_CONFIG_TRANSLATION;
            ps2_write(PS2_CMD_PORT, 0x60);
            ps2_write(PS2_DATA_PORT, configuration_byte);
        }
        init_keyboard(port);
    }
}

void init_and_test_I8042_controller(){
    
    ps2_write(PS2_CMD_PORT, DISABLE_FIRST_PORT);
    ps2_write(PS2_CMD_PORT, DISABLE_SECOND_PORT);
    ps2_flush_output();

    // update the controller config
    wait_write_signal();
    outb(PS2_CMD_PORT,READ_CONTROLLER_CONFIG);
    uint8_t config = ps2_read(PS2_DATA_PORT);
    config &= ~(PS2_CONFIG_IRQ1 | PS2_CONFIG_IRQ2 | PS2_CONFIG_TRANSLATION);
    ps2_write(PS2_CMD_PORT,WRITE_CONTROLLER_CONFIG);
    ps2_write(PS2_DATA_PORT,config);

    // some tests to look more professional
    uint8_t result;

    ps2_write(PS2_CMD_PORT,TEST_PS2_CONTROLLER);
    result = ps2_read(PS2_DATA_PORT);
    if (result == PS2_CONTROLLER_TEST_FAILED) error("PS/2 controller failed test");
    if (result == PS2_CONTROLLER_TEST_PASSED) log("PS/2 controller passed test");

    ps2_write(PS2_CMD_PORT,WRITE_CONTROLLER_CONFIG);
    ps2_write(PS2_DATA_PORT,config);

    // test if port 2 is supported
    ps2_write(PS2_CMD_PORT,ENABLE_SECOND_PORT);
    ps2_write(PS2_CMD_PORT,READ_CONTROLLER_CONFIG);
    config = ps2_read(PS2_DATA_PORT);
    uint8_t dual_channel = (config & PS2_CONFIG_CLOCK2) == 0;
    if (dual_channel) ps2_write(PS2_CMD_PORT, DISABLE_SECOND_PORT);

    ps2_write(PS2_CMD_PORT, TEST_FIRST_PORT);
    g_port_one_operational = ps2_read(PS2_DATA_PORT) == PORT_TEST_PASSED;
    g_port_two_operational = 0;
    if (dual_channel){
        ps2_write(PS2_CMD_PORT,TEST_SECOND_PORT);
        g_port_two_operational = ps2_read(PS2_DATA_PORT) == PORT_TEST_PASSED;
    }

    ps2_write(PS2_CMD_PORT,READ_CONTROLLER_CONFIG);
    config = ps2_read(PS2_DATA_PORT);
    if (g_port_one_operational) config |= PS2_CONFIG_IRQ1;
    if (g_port_two_operational) config |= PS2_CONFIG_IRQ2;
    ps2_write(PS2_CMD_PORT,WRITE_CONTROLLER_CONFIG);
    ps2_write(PS2_DATA_PORT,config);

    if (g_port_one_operational) {
        ps2_write(PS2_CMD_PORT,ENABLE_FIRST_PORT);
        if (!ps2_port_init(PS2_PORT_ONE)) g_port_one_operational = 0;
        ps2_write(PS2_CMD_PORT,DISABLE_FIRST_PORT);
    }
    
    if (g_port_two_operational) {
        ps2_write(PS2_CMD_PORT,ENABLE_SECOND_PORT);
        if (!ps2_port_init(PS2_PORT_TWO)) g_port_two_operational = 0;
        ps2_write(PS2_CMD_PORT,DISABLE_SECOND_PORT);
    }

    if (g_port_one_operational) log("PS/2 port 1 is operational"); else error("PS/2 port 1 is NOT operational");
    if (g_port_two_operational) log("PS/2 port 2 is operational"); else error("PS/2 port 2 is NOT operational");

    if (g_port_one_operational) ps2_port_init_driver(PS2_PORT_ONE);
    if (g_port_two_operational) ps2_port_init_driver(PS2_PORT_TWO);

}