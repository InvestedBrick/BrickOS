
#ifndef INCLUDE_STDIO_H
#define INCLUDE_STDIO_H

#include <stdint.h>
void print(unsigned char* str);
void printf(unsigned char* fmt,...);
void print_uint(uint32_t num);
int read_input(char* buffer,uint32_t buffer_size);
void debugf(unsigned char* fmt, ...);
#endif