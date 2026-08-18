#include "time.h"
#include <stdint.h>

void parse_unix_timestamp(uint64_t timestamp,date_t* date){
    uint16_t curr_year = 1970;

    uint32_t n_days = timestamp / (60 * 60 * 24);

    while(n_days > 364){

        if ((curr_year % 4 == 0 && curr_year % 100 != 0) || (curr_year % 400 == 0)) n_days -= 366; //leap year
        else n_days-= 365;

        curr_year++;
    }

    uint8_t is_leap_year = 0;
    if (curr_year % 4 == 0 && curr_year % 100 != 0) is_leap_year = 1; // if you encounter a date mismatch in the year 2400, then why tf are you using this OS?

    date->year = curr_year;

    uint8_t month_days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    uint8_t curr_month = 0;
    while (n_days > month_days[curr_month]){
        n_days -= month_days[curr_month];
        if (curr_month == 1 && is_leap_year) n_days--;
        curr_month++;
    }

    date->month = (curr_month + 1) % 12;
    if (n_days + 1 > month_days[curr_month]){
        date->day = 1;
        date->month = (date->month + 1) % 12;
    }else{
        date->day = n_days + 1; // current day is off by one
    }

    uint64_t seconds_left = timestamp % (60 * 60 * 24);
    uint8_t current_hour = 0;

    while (seconds_left > 3600){ seconds_left -= 3600;current_hour++;}
    date->hour = current_hour;

    uint8_t current_min = 0;
    while (seconds_left > 60){ seconds_left -= 60;current_min++;}
    date->minute = current_min;
    date->second = seconds_left;

}