#include "util.h"

void* memset(void* dest, int val, unsigned int count){
    unsigned char* p = dest;
    for(unsigned int i = 0; i < count;i++){
        p[i] = (unsigned char)val;
    }
    return dest;
}