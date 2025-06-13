
#ifndef INCLUDE_LOG_H
#define INCLUDE_LOG_H

#include "io.h"

void serial_write_with_prefix(const unsigned char* prefix, const unsigned char* msg,unsigned short com);

/**
 * log - logs a message to the COM1 serial port
 * @param msg The message to log 
 */
void log(const unsigned char* msg);

/**
 * warn - logs a warning to the COM1 serial port
 * @param msg The warning
 */
void warn(const unsigned char* msg);

/**
 * error - logs an error to the COM1 serial port
 * @param msg The error
 */
void error(const unsigned char* msg);

/**
 * log_uint:
 * Writes an unsigned integer to the COM1 serial port
 * 
 * @param num The integer
 */
void log_uint(unsigned int num);
#endif