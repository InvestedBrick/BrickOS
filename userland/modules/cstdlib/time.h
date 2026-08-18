#ifndef INCLUDE_TIME_H
#define INCLUDE_TIME_H

#include <stdint.h>

typedef struct {
    uint16_t year;
    uint8_t month; // 1 == Jan, 2 == Feb ...
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
}date_t;

void parse_unix_timestamp(uint64_t timestamp,date_t* date);

#endif