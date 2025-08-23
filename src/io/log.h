
#ifndef INCLUDE_LOG_H
#define INCLUDE_LOG_H

#include "io.h"

void serial_write_with_prefix(const uint8_t* prefix, const uint8_t* msg,uint16_t com);

/**
 * log - logs a message to the COM1 serial port
 * @param msg The message to log 
 */
void log(const uint8_t* msg);

/**
 * warn - logs a warning to the COM1 serial port
 * @param msg The warning
 */
void warn(const uint8_t* msg);

/**
 * error - logs an error to the COM1 serial port
 * @param msg The error
 */
void error(const uint8_t* msg);

/**
 * log_uint:
 * Writes an uint32_teger to the COM1 serial port
 * 
 * @param num The integer
 */
void log_uint(uint32_t num);

/**
 * panic: 
 * Something went really wrong, spins in place eternally
 */
void panic(const uint8_t* msg);
#endif