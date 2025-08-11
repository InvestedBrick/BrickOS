#include "cstdlib/stdio.h"
#include "cstdlib/stdutils.h"
#include "cstdlib/syscalls.h"
#include "cstdlib/malloc.h"
void main(){
    print("\nHello from module\n");
    unsigned char* line = (unsigned char*)malloc(SCREEN_COLUMNS + 1);
    unsigned char* dir_buffer = (unsigned char*)malloc(100);
    while(1){
        memset(line,0x0,SCREEN_COLUMNS + 1);
        print("\n|-(");
        getcwd(dir_buffer,100);
        print(dir_buffer);
        print(")-> ");
        int read_bytes = read_input(line,SCREEN_COLUMNS);
        line[read_bytes] = '\0';
        exit(1);
    }

    exit(1);
}