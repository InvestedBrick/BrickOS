#pragma once

#ifndef INCLUDE_IO_H
#define INCLUDE_IO_H

#define FB_COMMAND_PORT         0x3D4
#define FB_DATA_PORT            0x3D5

#define FB_HIGH_BYTE_COMMAND    14
#define FB_LOW_BYTE_COMMAND     15

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
#endif