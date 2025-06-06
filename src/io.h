#pragma once

#ifndef INCLUDE_IO_H
#define INCLUDE_IO_H

/* SCREEN I/O */
#define FB_COMMAND_PORT                 0x3D4
#define FB_DATA_PORT                    0x3D5
   
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
#endif