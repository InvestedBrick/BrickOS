#include "stdio.h"
#include "stdutils.h"
#include "syscalls.h"

void print(const char* str){
    unsigned int str_len = strlen(str);

    write(FD_STDOUT,str,str_len);
}

int read_input(char* buffer,unsigned int buffer_size){
    int read_chars = 0;
    char c;
    while(1){
        if (read(FD_STDIN,&c,1) == 1){
            if (read_chars >= buffer_size || c == '\n')
                break;

            buffer[read_chars++] = c;

            write(FD_STDOUT,&c,1);
        }
    }

    return read_chars;
    
}