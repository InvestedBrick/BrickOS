#include "log.h"

void serial_write_with_prefix(const unsigned char* prefix, const unsigned char* msg,unsigned short com){
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