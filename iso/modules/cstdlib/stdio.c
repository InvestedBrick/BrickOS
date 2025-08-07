#include "stdio.h"
#include "stdutils.h"
#include "syscalls.h"

void print(const char* str){
    unsigned int str_len = strlen(str);

    write(FD_STDOUT,str,str_len);
}

int read_input(char* buffer, unsigned int buffer_size) {
    char temp_buf[TEMP_BUF_SIZE];
    int total_read = 0;

    while (total_read < buffer_size) {
        int max_read = (buffer_size - total_read < TEMP_BUF_SIZE)
                       ? (buffer_size - total_read)
                       : TEMP_BUF_SIZE;

        int bytes_read = read(FD_STDIN, temp_buf, max_read);
        if (bytes_read <= 0)
            break;

        for (int i = 0; i < bytes_read && total_read < buffer_size; ++i) {
            char c = temp_buf[i];

            if (c == '\n') {
                write(FD_STDOUT, &c, 1); 
                return total_read;
            }

            buffer[total_read++] = c;

            write(FD_STDOUT, &c, 1);
        }
    }

    return total_read;
}