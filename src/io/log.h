#pragma once

#ifndef INCLUDE_LOG_H
#define INCLUDE_LOG_H

#include "io.h"

void serial_write_with_prefix(const unsigned char* prefix, const unsigned char* msg,unsigned short com){
    serial_write(prefix,com);
    serial_write(msg,com);
    serial_write("\n",com);
}

/**
 * log - logs a message to the COM1 serial port
 * @param msg The message to log 
 */
void log(const unsigned char* msg){
    serial_write_with_prefix("[LOG]   ",msg,SERIAL_COM1_BASE);
}

/**
 * warn - logs a warning to the COM1 serial port
 * @param msg The warning
 */
void warn(const unsigned char* msg){
    serial_write_with_prefix("[WARN]  ",msg,SERIAL_COM1_BASE);

}

/**
 * error - logs an error to the COM1 serial port
 * @param msg The error
 */
void error(const unsigned char* msg){
    serial_write_with_prefix("[ERROR] ",msg,SERIAL_COM1_BASE);

}

#endif