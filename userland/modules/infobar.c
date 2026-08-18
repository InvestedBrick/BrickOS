
#include "cstdlib/window.h"
#include "cstdlib/time.h"
#define VBE_COLOR_BLACK 0xff000000
#define VBE_COLOR_GRAY  0xffaaaaaa

unsigned char* major_minor_to_decimal(uint32_t major, uint32_t minor, unsigned char* ending) {
    unsigned char* major_str = uint32_to_ascii(major);
    uint32_t major_strlen = strlen(major_str);
    unsigned char* minor_str = uint32_to_ascii(minor);
    uint32_t minor_strlen = strlen(minor_str);
    uint32_t ending_strlen = strlen(ending);
    uint32_t total_str_size = major_strlen + minor_strlen + ending_strlen + 2;// +2  one for . and one for \0
    unsigned char* result = malloc(total_str_size);
    memset(result, 0, total_str_size);
    memcpy(result,major_str,major_strlen);
    result[major_strlen] = '.';
    memcpy(&result[major_strlen + 1],minor_str,minor_strlen);
    memcpy(&result[major_strlen + 1 + minor_strlen],ending,ending_strlen);
    free(major_str);
    free(minor_str);
    return result;
}

unsigned char* large_bytenum_to_decimal_str(uint32_t num){
    uint32_t gb = num / (1024 * 1024 * 1024);
    uint32_t mb = num / (1024 * 1024);
    uint32_t kb = num / 1024;
    if (gb > 0){
        return major_minor_to_decimal(gb, mb % 1024, " GiB");
    }else if (mb > 0){
        // return fmt in MiB
        return major_minor_to_decimal(mb, kb % 1024, " MiB");
    }else if (kb > 0){
        // return fmt in KiB
        return major_minor_to_decimal(kb, num % 1024, " KiB");
    }

    return major_minor_to_decimal(num, 0, " B");

}

void clear_screen(user_fb_t* fb){
    for (uint32_t i = 0; i < fb->win_height; i++) {
        for (uint32_t j = 0; j < fb->win_width; j++) {
            write_pixel(fb, j, i, VBE_COLOR_BLACK);
        }
    }
    fb->cursor_x = 1;
    fb->cursor_y = 1;
}


void print_string(user_fb_t* fb, unsigned char* str) {
    for (uint32_t i = 0; str[i]; i++) {
        write_char(fb, str[i], VBE_COLOR_GRAY,VBE_COLOR_BLACK);
        fb->cursor_x++;
    }   
}

void print_meminfo(user_fb_t* fb) {
    unsigned char buf[128] = {0};
    int meminfo = open("sysinfo/meminfo",FILE_FLAG_NONE);
    memset(buf,0x0,sizeof(buf));
    int bytes_read = read(meminfo,buf,sizeof(buf));
    uint32_t split = find_char(buf,'-');
    if (split == (uint32_t)-1) {close(meminfo);return;}
    buf[split] = '\0';
    uint32_t used_pages = ascii_to_uint32(buf);
    uint32_t total_pages = ascii_to_uint32(&buf[split + 1]);

    unsigned char* used_percent = uint32_to_ascii((used_pages * 100) / total_pages);
    unsigned char* used_str = large_bytenum_to_decimal_str(used_pages * 0x1000);
    unsigned char* total_str = large_bytenum_to_decimal_str(total_pages * 0x1000);

    print_string(fb,"Memory: ");
    print_string(fb,used_str);
    print_string(fb,"/");
    print_string(fb,total_str);
    print_string(fb," (");
    print_string(fb,used_percent);
    print_string(fb,"%)");
    free(used_percent);
    free(used_str);
    free(total_str);
    close(meminfo);
}
void print_diskinfo(user_fb_t* fb) {
    unsigned char buf[128] = {0};
    int diskinfo = open("sysinfo/diskinfo",FILE_FLAG_NONE);
    memset(buf,0x0,sizeof(buf));
    int bytes_read = read(diskinfo,buf,sizeof(buf));
    uint32_t split = find_char(buf,'-');
    if (split == (uint32_t)-1) {close(diskinfo);return;}
    buf[split] = '\0';
    uint32_t used_sectors = ascii_to_uint32(buf);
    uint32_t total_sectors = ascii_to_uint32(&buf[split + 1]);
    
    unsigned char* used_str = large_bytenum_to_decimal_str(used_sectors * 512);
    unsigned char* total_str = large_bytenum_to_decimal_str(total_sectors * 512);
    unsigned char* disk_used_percent = uint32_to_ascii((used_sectors * 100) / total_sectors);

    print_string(fb,"Disk: ");
    print_string(fb,used_str);
    print_string(fb,"/");
    print_string(fb,total_str);
    print_string(fb," (");
    print_string(fb,disk_used_percent);
    print_string(fb,"%)");
    free(disk_used_percent);
    free(used_str);
    free(total_str);
    close(diskinfo);
}
void print_cpuinfo(user_fb_t* fb) {
    unsigned char buf[128] = {0};
    int cpuinfo = open("sysinfo/cpuinfo",FILE_FLAG_NONE);
    memset(buf,0x0,sizeof(buf));
    int bytes_read = read(cpuinfo,buf,sizeof(buf));
    print_string(fb,"CPU: ");
    print_string(fb,buf);
    close(cpuinfo);
}
void print_uptime(user_fb_t* fb) {
    unsigned char buf[128] = {0};
    int uptime = open("sysinfo/uptime",FILE_FLAG_NONE);
    int bytes_read = read(uptime,buf,sizeof(buf));
    uint32_t uptime_seconds = ascii_to_uint32(buf);
    uint32_t hours = uptime_seconds / 3600;
    uint32_t minutes = (uptime_seconds % 3600) / 60;
    uint32_t seconds = uptime_seconds % 60;
    print_string(fb,"Uptime: ");
    if (hours > 0) {
        unsigned char* hours_str = uint32_to_ascii(hours);
        print_string(fb,hours_str);
        print_string(fb,"h ");
        free(hours_str);
    }
    if (minutes > 0) {
        unsigned char* minutes_str = uint32_to_ascii(minutes);
        print_string(fb,minutes_str);
        print_string(fb,"m ");
        free(minutes_str);
    }
    unsigned char* seconds_str = uint32_to_ascii(seconds);
    print_string(fb,seconds_str);
    print_string(fb,"s");
    free(seconds_str);
    close(uptime);
}

void print_datetime(user_fb_t* fb){
    uint64_t timestamp = gettimeofday();
    date_t date;
    parse_unix_timestamp(timestamp,&date);
    char* months[] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
    char* day_str = uint32_to_ascii(date.day);
    char* year_str = uint32_to_ascii(date.year);
    print_string(fb,months[date.month - 1]);
    print_string(fb," ");
    print_string(fb,day_str);
    print_string(fb,", ");
    print_string(fb,year_str);
    free(day_str);
    free(year_str);

    char* hour_str = uint32_to_ascii(date.hour);
    char* minute_str = uint32_to_ascii(date.minute);
    print_string(fb," ");
    if (date.hour < 10) print_string(fb,"0");
    print_string(fb,hour_str);
    print_string(fb,":");
    if (date.minute < 10) print_string(fb,"0");
    print_string(fb,minute_str);
    free(hour_str);
    free(minute_str);
}

int main(){
    user_fb_t infobar_fb;
    request_window(&infobar_fb, 1280,3 * CHAR_HEIGHT);
    if (!infobar_fb.fb) exit(2);

    while(1){
        clear_screen(&infobar_fb);
        print_datetime(&infobar_fb);
        print_string(&infobar_fb, " | ");
        print_uptime(&infobar_fb);
        print_string(&infobar_fb, " | ");
        print_meminfo(&infobar_fb);
        print_string(&infobar_fb, " | ");
        print_diskinfo(&infobar_fb);
        commit_window();
        mssleep(1000);
    }
}