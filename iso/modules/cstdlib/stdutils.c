#include "stdutils.h"

unsigned int strlen(const char* str){
    unsigned int len = 0;
    while(str[len] != 0){len++;}
    return len;
}

void* memset(void* dest, int val, unsigned int n){
    unsigned char* p = dest;
    for(unsigned int i = 0; i < n;i++){
        p[i] = (unsigned char)val;
    }
    return dest;
}

void loop(){
    while(1){};
}