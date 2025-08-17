#include "stdio.h"
#include "stdutils.h"
#include "syscalls.h"

void print(const char* str){
    unsigned int str_len = strlen(str);

    write(FD_STDOUT,str,str_len);
}
void print_uint(unsigned int num){
    //convert num to ASCII
    char ascii[11] = {0};
    if(num == 0){
        print("0");
        return;
    }
    unsigned int temp,count = 0,i;
    for(temp=num;temp != 0; temp /= 10,count++);
    for (i = count - 1, temp=num;i < 0xffffffff;i--){
        ascii[i] = (temp%10) + 0x30;
        temp /= 10;
    }
    
    print(ascii);
}
int read_input(char* buffer,unsigned int buffer_size){
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