
#ifndef INCLUDE_STDIO_H
#define INCLUDE_STDIO_H

#define SCREEN_ROWS 25
#define SCREEN_COLUMNS 80

void print(const char* str);

int read_input(char* buffer,unsigned int buffer_size);
#endif