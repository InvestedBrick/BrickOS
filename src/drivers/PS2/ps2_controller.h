#ifndef INLCUDE_PS2_CONTROLLER_H
#define INLCUDE_PS2_CONTROLLER_H


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

#define PS2_WRITE_NEXT_BYTE_TO_SECOND_PORT 0xd4

/**
 * await_read_signal:
 * Waits until the output buffer byte of the PS/2 controller is written
 * 
 * Call before reading from the PS/2 data port (0x60)
 */
void await_read_signal();

/**
 * await_write_signal:
 * Waits until the input buffer byte of the PS/2 controller is clear
 * 
 * Call before writing to the PS/2 data or command port (0x60/0x64)
 */
void await_write_signal();

/**
 * init_and_test_I8042_controller:
 * Initializes the I8042 PS/2 controller by enabling both PS/2 ports, enabling interrupts for them and testing the ports and controller itself
 */
void init_and_test_I8042_controller();
/**
 * ps2_flush_output:
 * Consumes all still pending PS/2 data
 */
void ps2_flush_output();
#endif