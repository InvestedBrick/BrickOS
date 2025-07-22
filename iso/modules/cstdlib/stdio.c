#include "stdio.h"
#include "stdutils.h"
#include "syscalls.h"

void print(const char* str){
    unsigned int str_len = strlen(str);

    write(FD_STDOUT,str,str_len);
}