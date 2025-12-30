#include "log.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include "../utilities/util.h"
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

void log_uint64(uint64_t num){
    char ascii[21]; /* max uint64 = 20 digits */
    memset(ascii,0x0,21);
    if(num == 0){
        log((const unsigned char*)"0");
        return;
    }
    uint64_t temp = num;
    int count = 0;
    while(temp != 0){
        temp /= 10;
        count++;
    }
    temp = num;
    for(int i = count - 1; i >= 0; --i){
        ascii[i] = (char)((temp % 10) + '0');
        temp /= 10;
    }
    log((const unsigned char*)ascii);
}

/* Write 64-bit number as hex with 0x prefix */
void log_hex64(uint64_t num){
    char buf[2 + (2 * sizeof(uint64_t)) + 1]; /* "0x" + 16 hex + nul */
    const char *hex = "0123456789abcdef";
    int idx = 0;
    buf[idx++] = '0';
    buf[idx++] = 'x';
    int started = 0;
    for(int shift = (int)(sizeof(uint64_t) * 8 - 4); shift >= 0; shift -= 4){
        uint8_t nib = (uint8_t)((num >> shift) & 0xF);
        if(nib != 0 || started || shift == 0){
            started = 1;
            buf[idx++] = hex[nib];
        }
    }
    buf[idx] = '\0';
    log((const unsigned char*)buf);
}

/* Simple, small vsnprintf-like implementation used by logf/warnf/errorf.
 * Supports: %s, %c, %u, %d, %x, %p, %%
 * No width/precision handling. Buffer is always NUL-terminated.
 */
static int simple_vsnprintf(char *buf, size_t bufsz, const char *fmt, va_list ap){
    size_t idx = 0;
    if(bufsz == 0) return 0;
    while(*fmt && idx < bufsz - 1){
        if(*fmt != '%'){
            buf[idx++] = *fmt++;
            continue;
        }
        fmt++; 
        if(*fmt == '\0') break;
        if(*fmt == '%'){
            buf[idx++] = '%';
            fmt++;
            continue;
        }
        switch(*fmt){
            case 's': {
                const char *s = va_arg(ap, const char*);
                if(!s) s = "(null)";
                while(*s && idx < bufsz - 1) buf[idx++] = *s++;
                break;
            }
            case 'c': {
                int c = va_arg(ap, int);
                if(idx < bufsz - 1) buf[idx++] = (char)c;
                break;
            }
            case 'u': {
                unsigned long v = va_arg(ap, unsigned long);
                char tmp[32];
                int t = 0;
                if(v == 0) tmp[t++] = '0';
                while(v && t < (int)sizeof(tmp)) {
                    tmp[t++] = '0' + (v % 10);
                    v /= 10;
                }
                while(t > 0 && idx < bufsz - 1) buf[idx++] = tmp[--t];
                break;
            }
            case 'd': {
                long v = va_arg(ap, long);
                unsigned long uv;
                if(v < 0){
                    if(idx < bufsz - 1) buf[idx++] = '-';
                    uv = (unsigned long)(-v);
                } else uv = (unsigned long)v;
                char tmp[32];
                int t = 0;
                if(uv == 0) tmp[t++] = '0';
                while(uv && t < (int)sizeof(tmp)){
                    tmp[t++] = '0' + (uv % 10);
                    uv /= 10;
                }
                while(t > 0 && idx < bufsz - 1) buf[idx++] = tmp[--t];
                break;
            }
            case 'x': {
                unsigned long v = va_arg(ap, unsigned long);
                char tmp[32];
                int t = 0;
                const char *hex = "0123456789abcdef";
                buf[idx++] = '0';
                buf[idx++] = 'x';
                if(v == 0) tmp[t++] = '0';
                while(v && t < (int)sizeof(tmp)){
                    tmp[t++] = hex[v & 0xF];
                    v >>= 4;
                }
                while(t > 0 && idx < bufsz - 1) buf[idx++] = tmp[--t];
                break;
            }
            case 'p': {
                void *p = va_arg(ap, void*);
                unsigned long v = (unsigned long)p;
                if(idx < bufsz - 1) buf[idx++] = '0';
                if(idx < bufsz - 1) buf[idx++] = 'x';
                char tmp[2 * sizeof(void*) + 1];
                int t = 0;
                const char *hex = "0123456789abcdef";
                if(v == 0) tmp[t++] = '0';
                while(v && t < (int)sizeof(tmp)){
                    tmp[t++] = hex[v & 0xF];
                    v >>= 4;
                }
                while(t > 0 && idx < bufsz - 1) buf[idx++] = tmp[--t];
                break;
            }
            default:
                /* Unknown specifier: emit it as-is (including the '%') */
                if(idx < bufsz - 1) buf[idx++] = '%';
                if(idx < bufsz - 1) buf[idx++] = *fmt;
                break;
        }
        fmt++;
    }
    buf[idx] = '\0';
    return (int)idx;
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