#include "log.h"

void serial_write_with_prefix(const unsigned char* prefix, const unsigned char* msg,uint16_t com){
    serial_write(prefix,com);
    serial_write(msg,com);
    serial_write("\n",com);
}

void log(const unsigned char* msg){
    serial_write_with_prefix("[LOG] ",msg,SERIAL_COM1_BASE);
}

void warn(const unsigned char* msg){
    serial_write_with_prefix("[WARN] ",msg,SERIAL_COM1_BASE);
}

void error(const unsigned char* msg){
    serial_write_with_prefix("[ERROR] ",msg,SERIAL_COM1_BASE);
}

void log_uint32(uint32_t num){
    //convert num to ASCII
    char ascii[11] = {0};
    if(num == 0){
        log("0");
        return;
    }
    uint32_t temp,count = 0,i;
    for(temp=num;temp != 0; temp /= 10,count++);
    for (i = count - 1, temp=num;i < 0xffffffff;i--){
        ascii[i] = (temp%10) + 0x30;
        temp /= 10;
    }
    
    log(ascii);
}

void panic(const unsigned char* msg){
    serial_write_with_prefix("[PANIC] ",msg,SERIAL_COM1_BASE);
    while(1){};
}