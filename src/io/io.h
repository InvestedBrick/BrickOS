#pragma once

#ifndef INCLUDE_IO_H
#define INCLUDE_IO_H

/* SCREEN I/O */
#define FB_COMMAND_PORT                 0x3d4
#define FB_DATA_PORT                    0x3d5
   
#define FB_HIGH_BYTE_COMMAND            14
#define FB_LOW_BYTE_COMMAND             15

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

void outb(unsigned short port, unsigned char data);

/**
 * 
 * inb - recieves byte from I/O port
 * @param port ID of port
 * @return recieved byte
 */
unsigned char inb(unsigned short port);

/**
 * serial_configure_baud_rate:
 * sets speed of data being sent
 * base speed is 115299 bits/s
 * resulting speed is (115299 / divisor) bits/s
 * 
 * @param com The COM port to configure
 * @param divisor The divisor
 */
void serial_configure_baud_rate(unsigned short com, unsigned short divisor);

/** serial_configure_line:
 *  Configures the line of the given serial port. The port is set to have a
 *  data length of 8 bits, no parity bits, one stop bit and break control
 *  disabled.
 *
 *  @param com  The serial port to configure
 */
void serial_configure_line(unsigned short com);

/**
 * serial_configure_buffer:
 * Configures the buffer of the given serial port. The port enables FIFO, clears reciever and transmission FIFO queues
 * and uses 14 bytes as the queue size
 * 
 * @param com The serial port to configure
 */
void serial_configure_buffer(unsigned short com);


/**
 * serial_configure_modem:
 * Configures the modem for the given serial port
 * Sets it up to be ready for transmission and ready for data terminal
 * 
 * @param com The serial port to configure
 */
void serial_configure_modem(unsigned short com);

/**
 * serial_is_transmit_fifo_empty:
 * Checks wether the transmit FIFO queue is empty or not for the givem COM port
 * 
 * @param com The COM port
 * @return 0 if the transmit FIFO queue is not empty 
 *         1 if the transmit FIFO queue is emtpy
 */
int serial_is_transmit_fifo_empty(unsigned int com);

/** 
 * serial_write:
 * writes a null-terminated string to a given COM port
 * 
 * @param data The string ptr
 * @param com The COM port
*/
void serial_write(const unsigned char* data, unsigned short com);

#endif