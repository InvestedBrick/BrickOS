#ifndef INCLUDE_LOG_H
#define INCLUDE_LOG_H

#include "io.h"
#include <stdint.h>

void serial_write_with_prefix(const unsigned char* prefix, const unsigned char* msg,uint16_t com);

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
 * log_uint64:
 * Writes an unsigned 64-bit integer to the COM1 serial port
 * 
 * @param num The integer
 */
void log_uint64(uint64_t num);

/**
 * log_hex64:
 * Writes an unsigned 64-bit integer as hex (0x...) to the COM1 serial port
 *
 * @param num The integer
 */
void log_hex64(uint64_t num);

/**
 * printf-style helpers
 */
void logf(const unsigned char* fmt, ...);
void warnf(const unsigned char* fmt, ...);
void errorf(const unsigned char* fmt, ...);

/**
 * panic: 
 * Something went really wrong, spins in place eternally
 */
void panic(const unsigned char* msg);
#endif