#include <shared/format.h>

/* Simple, small vsnprintf-like implementation used by logf/warnf/errorf.
 * Supports: %s, %c, %u, %d, %x, %p, %%
 * No width/precision handling. Buffer is always NULL-terminated.
 */
int simple_vsnprintf(char *buf, size_t bufsz, const char *fmt, va_list ap){
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
                unsigned int v = va_arg(ap, unsigned int);
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
                int v = va_arg(ap, int);
                unsigned int uv;
                if(v < 0){
                    if(idx < bufsz - 1) buf[idx++] = '-';
                    uv = (unsigned int)(-v);
                } else uv = (unsigned int)v;
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

void write_bufferf(unsigned char* buf, uint32_t buf_size, unsigned char* fmt, ...){
    va_list ap;
    va_start(ap, fmt);
    simple_vsnprintf(buf, buf_size, (const char*)fmt, ap);
    va_end(ap);
}
