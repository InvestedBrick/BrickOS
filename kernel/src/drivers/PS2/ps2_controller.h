#ifndef INCLUDE_PS2_CONTROLLER_H
#define INCLUDE_PS2_CONTROLLER_H

#define TIMEOUT 100000
#define ACK 0xfa

#define PS2_DATA_PORT 0x60
#define PS2_CMD_PORT 0x64
#define PS2_STATUS_PORT 0x64

#define READ_CONTROLLER_CONFIG 0x20
#define WRITE_CONTROLLER_CONFIG 0x60

#define DISABLE_FIRST_PORT 0xad
#define ENABLE_FIRST_PORT 0xae

#define DISABLE_SECOND_PORT 0xa7
#define ENABLE_SECOND_PORT 0xa8

#define TEST_SECOND_PORT 0xa9
#define TEST_FIRST_PORT 0xab
#define TEST_PS2_CONTROLLER 0xaa

#define PS2_CONTROLLER_TEST_PASSED 0x55
#define PS2_CONTROLLER_TEST_FAILED 0xfc

#define PORT_TEST_PASSED 0x00
#define PORT_TEST_FAILED 0x01 

#define PS2_WRITE_NEXT_BYTE_TO_SECOND_PORT 0xd4

#define PS2_CONFIG_TRANSLATION (1 << 6)
#define PS2_CONFIG_IRQ1 (1 << 0)
#define PS2_CONFIG_IRQ2 (1 << 1)
#define PS2_CONFIG_CLOCK1 (1 << 4)
#define PS2_CONFIG_CLOCK2 (1 << 5)

#define PORT_GET_DEV_ID 0xf2
#define PORT_SET_SAMPLE_RATE 0xf3
#define PORT_ENABLE_DATA_REPORT 0xf4
#define PORT_DISABLE_DATA_REPORT 0xf5
#define PORT_SET_DEFAULTS 0xf6
#define PORT_RESEND 0xfe
#define PS2_PORT_RESET 0xff 

#define PS2_KB_ID1 0xAB
#define PS2_KB_ID2 0x83

#define PS2_MOUSE_ID1 0x0
#define PS2_MOUSE_ID2 0x0

typedef enum {
    PS2_PORT_ONE = 0,
    PS2_PORT_TWO = 1
} ps2_ports_t;

#include <stdint.h>

/**
 * ps2_port_write:
 * Writes a value to a PS/2 port
 * @param port The PS/2 port 
 * @param value the value to write to the specified port
 * @return Whether the write was successfull
 */
uint8_t ps2_port_write(ps2_ports_t port, uint8_t value);

/**
 * ps2_port_read:
 * Reads a value from the PS/2 data port
 * @param wait A boolean to determine if to await the read signal
 * @return The read data
 */
uint8_t ps2_port_read(uint8_t wait);

/**
 * ps2_port_enable:
 * Enables a PS/2 port
 * @param port The port to enable
 */
void ps2_port_enable(ps2_ports_t port);

/**
 * ps2_port_disable:
 * Disables a PS/2 port
 * @param port The port to disable
 */
void ps2_port_disable(ps2_ports_t port);

/**
 * init_and_test_I8042_controller:
 * Initializes the I8042 PS/2 controller and tries to set up both ports so that port 1 is the keyboard and port 2 is the mouse
 */
void init_and_test_I8042_controller();
#endif