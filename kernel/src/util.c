#include "util.h"
#include "memory/kmalloc.h"
#include "io/log.h"

void* memmove(void* dest, void* src, uint32_t n) {
    if (n == 0) return dest;

    unsigned char* _dest = dest;
    unsigned char* _src = src;

    if (_dest < _src) {
        // Forward copy
        for (uint32_t i = 0; i < n; i++) {
            _dest[i] = _src[i];
        }
    } else if (_dest > _src) {
        // Backward copy
        for (uint32_t i = n; i > 0; i--) {
            _dest[i - 1] = _src[i - 1];
        }
    }

    return dest;
}

uint8_t streq(const unsigned char* str1, const unsigned char* str2){
    uint32_t i = 0;
    while(1){
        if (str1[i] != str2[i]) return 0;

        if (str1[i] == '\0' && str2[i] == '\0') return 1;

        i++;
    }

    //how did we get here?
    return 1;

}
uint8_t strneq(const unsigned char* str1, const unsigned char* str2, uint32_t len_1, uint32_t len_2){
    uint32_t max_len = len_1 > len_2 ? len_1 : len_2;

    for (uint32_t i = 0; i < max_len;i++){
        if (str1[i] != str2[i]) return 0;

        if (str1[i] == '\0' && str2[i] == '\0') return 1;
    }
    return 1;
}

void free_string_arr(string_array_t* str_arr){
    for (uint32_t i = 0; i < str_arr->n_strings;i++){
        kfree(str_arr->strings[i].str);
    }
    if (str_arr->n_strings > 0) kfree(str_arr->strings);
    kfree(str_arr);
}

uint32_t rfind_char(unsigned char* str, unsigned char c){
    uint32_t str_len = strlen(str);

    for (int i = str_len - 1; i >= 0; i--){
        if (str[i] == c) return (uint32_t)i;
    }

    return (uint32_t)-1;
}

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

    date->month = curr_month + 1;
    date->day = n_days + 1; // current day is off by one

    uint64_t seconds_left = timestamp % (60 * 60 * 24);
    uint8_t current_hour = 0;

    while (seconds_left > 3600){ seconds_left -= 3600;current_hour++;}
    date->hour = current_hour;

    uint8_t current_min = 0;
    while (seconds_left > 60){ seconds_left -= 60;current_min++;}
    date->minute = current_min;
    date->second = seconds_left;


}
