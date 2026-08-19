#include "log.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <shared/util.h>
#include <shared/format.h>
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


void logf(const unsigned char* fmt, ...){
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    simple_vsnprintf(buf, sizeof(buf), (const char*)fmt, ap);
    va_end(ap);
    log((const unsigned char*)buf);
}

void warnf(const unsigned char* fmt, ...){
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    simple_vsnprintf(buf, sizeof(buf), (const char*)fmt, ap);
    va_end(ap);
    warn((const unsigned char*)buf);
}

void errorf(const unsigned char* fmt, ...){
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    simple_vsnprintf(buf, sizeof(buf), (const char*)fmt, ap);
    va_end(ap);
    error((const unsigned char*)buf);
}

void panic(const unsigned char* msg){
    serial_write_with_prefix("[PANIC] ",msg,SERIAL_COM1_BASE);
    while(1){};
}