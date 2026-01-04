
#ifndef INCLUDE_IO_H
#define INCLUDE_IO_H
#include <stdint.h>
/* SCREEN I/O */
#define FB_COMMAND_PORT                 0x3d4
#define FB_DATA_PORT                    0x3d5
   
#define FB_HIGH_BYTE_COMMAND            14
#define FB_LOW_BYTE_COMMAND             15

#define FB_DISABLE_CURSOR_COMMAND 0x0A
#define FB_DISABLE_CURSOR_DATA 0x20

/* Serial I/O */

#define SERIAL_COM1_BASE                0x3f8
   
#define SERIAL_DATA_PORT(base)          (base)
#define SERIAL_FIFO_COMMAND_PORT(base)  (base + 2)
#define SERIAL_LINE_COMMAND_PORT(base)  (base + 3)
#define SERIAL_MODEM_COMMAND_PORT(base) (base + 4)
#define SERIAL_LINE_STATUS_PORT(base)   (base + 5)

/* SERIAL_LINE_ENABLE_DLAB:
 * Tells the serial port to expect first the highest 8 bits on the data port,
 * then the lowest 8 bits will follow
 */
#define SERIAL_LINE_ENABLE_DLAB         0x80

/**
 * outb - sends byte to I/O port
 * @param port ID of port 
 * @param data Byte to be sent
 * 
 */

void outb(uint16_t port, uint8_t data);

/**
 * outw - sends a word to I/O port
 * @param port ID of port
 * @param data word (uint16_t) to be sent
 */
void outw(uint16_t port, uint16_t data);

/**
 * outl - sends a dword to I/O port
 * @param port ID of port
 * @param data dword (uint32_t) to be sent
 */
void outl(uint16_t port,uint32_t data);

/**
 * 
 * inb - recieves byte from I/O port
 * @param port ID of port
 * @return recieved byte
 */
uint8_t inb(uint16_t port);
/**
 * inw - recieves a word from I/O port
 * @param port ID of port
 * @return recieved word (uint16_t)
 */
uint16_t inw(uint16_t port);

/**
 * inl - Recieves a dword from I/O port
 * @param port ID of port
 * @return recieved dword (uint32_t) 
 */
uint32_t inl(uint16_t port);

/**
 * serial_configure_baud_rate:
 * sets speed of data being sent
 * base speed is 115299 bits/s
 * resulting speed is (115299 / divisor) bits/s
 * 
 * @param com The COM port to configure
 * @param divisor The divisor
 */
void serial_configure_baud_rate(uint16_t com, uint16_t divisor);

/** serial_configure_line:
 *  Configures the line of the given serial port. The port is set to have a
 *  data length of 8 bits, no parity bits, one stop bit and break control
 *  disabled.
 *
 *  @param com  The serial port to configure
 */
void serial_configure_line(uint16_t com);

/**
 * serial_configure_buffer:
 * Configures the buffer of the given serial port. The port enables FIFO, clears reciever and transmission FIFO queues
 * and uses 14 bytes as the queue size
 * 
 * @param com The serial port to configure
 */
void serial_configure_buffer(uint16_t com);


/**
 * serial_configure_modem:
 * Configures the modem for the given serial port
 * Sets it up to be ready for transmission and ready for data terminal
 * 
 * @param com The serial port to configure
 */
void serial_configure_modem(uint16_t com);

/**
 * serial_is_transmit_fifo_empty:
 * Checks wether the transmit FIFO queue is empty or not for the givem COM port
 * 
 * @param com The COM port
 * @return 0 if the transmit FIFO queue is not empty 
 *         1 if the transmit FIFO queue is emtpy
 */
int serial_is_transmit_fifo_empty(uint32_t com);

/** 
 * serial_write:
 * writes a null-terminated string to a given COM port
 * 
 * @param data The string ptr
 * @param com The COM port
*/
void serial_write(const unsigned char* data, uint16_t com);

#endif