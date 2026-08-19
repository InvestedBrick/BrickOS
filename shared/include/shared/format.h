#ifndef INCLUDE_FORMAT_H
#define INCLUDE_FORMAT_H

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

/**
 * write_bufferf:
 * Fancy formatting of strings into a buffer
 */
void write_bufferf(unsigned char* buf, uint32_t buf_size, unsigned char* fmt, ...);

int simple_vsnprintf(char *buf, size_t bufsz, const char *fmt, va_list ap);
#endif