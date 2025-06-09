#pragma once
#ifndef INCLUDE_UTIL_H
#define INCLUDE_UTIL_H

#define CEIL_DIV(a,b) (((a + b) - 1 )/ b)

void* memset(void* dest, int val, unsigned int count);
#endif