#include "stdutils.h"

unsigned int strlen(const char* str){
    unsigned int len = 0;
    while(str[len] != 0){len++;}
    return len;
}