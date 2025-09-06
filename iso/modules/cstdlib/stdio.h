
#ifndef INCLUDE_STDIO_H
#define INCLUDE_STDIO_H

#include <stdint.h>
void print(const char* str);
void print_uint(uint32_t num);
int read_input(char* buffer,uint32_t buffer_size);
#endif