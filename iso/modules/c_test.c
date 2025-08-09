#include "cstdlib/stdio.h"
#include "cstdlib/stdutils.h"
#include "cstdlib/syscalls.h"
#include "cstdlib/malloc.h"
void main(){
    print("\nHello from module\n");
    print("What is your name?: ");
    char* input_buffer = (char*)malloc(100);
    int bytes_read = read_input(input_buffer,80);
    input_buffer[bytes_read] = 0;
    print("\nHello ");
    print(input_buffer);
    print("\n");
    exit(1);
}