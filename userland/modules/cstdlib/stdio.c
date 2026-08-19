#include <stdint.h>

#include "stdio.h"
#include <shared/util.h>
#include <shared/format.h>
#include "syscalls.h"

void print(unsigned char* str){
    uint32_t str_len = strlen(str);

    write(FD_STDOUT,str,str_len);
}

void printf(unsigned char* fmt,...){
    unsigned char buf[256] = {0};
    va_list ap;
    va_start(ap,fmt);
    simple_vsnprintf(buf, sizeof(buf),(const char*) fmt,ap);
    va_end(ap);
    print(buf);
}

void debugf(unsigned char* fmt, ...){
    unsigned char buf[256] = {0};
    va_list ap;
    va_start(ap,fmt);
    simple_vsnprintf(buf, sizeof(buf),(const char*) fmt,ap);
    va_end(ap);
    debug(buf);
}

int read_input(char* buffer,uint32_t buffer_size){
    int read_chars = 0;
    char c;
    while(1){
        if (read(FD_STDIN,&c,1) == 1){
            if (read_chars >= buffer_size || c == '\n')
                break;

            if (c == '\b'){
                // Backspace
                if (read_chars > 0){
                    read_chars--;
                    write(FD_STDOUT,&c,1);
                }
                continue;
            }
            buffer[read_chars++] = c;

            write(FD_STDOUT,&c,1);
        }
    }

    return read_chars;
    
}