
#ifndef INCLUDE_STDIO_H
#define INCLUDE_STDIO_H

#define SCREEN_ROWS 25
#define SCREEN_COLUMNS 80
#include <stdint.h>
void print(const char* str);
void print_uint(uint32_t num);
int read_input(char* buffer,uint32_t buffer_size);
#endif