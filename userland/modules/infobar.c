
#include "cstdlib/window.h"
#include "cstdlib/time.h"
#include <shared/util.h>
#include <shared/format.h>
#define VBE_COLOR_BLACK 0xff000000
#define VBE_COLOR_GRAY  0xffaaaaaa

unsigned char* major_minor_to_decimal(uint32_t major, uint32_t minor, unsigned char* ending) {
    unsigned char buf[256] = {0};
    write_bufferf(buf,256,"%d.%d%s",major, minor,ending);
    uint32_t len = strlen(buf);
    unsigned char* result = (unsigned char*)malloc(len + 1);
    memcpy(result,buf,len + 1);
    
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

void print_stringf(user_fb_t* fb,unsigned char* fmt, ...){
    unsigned char buf[256] = {0};
    va_list ap;
    va_start(ap,fmt);
    simple_vsnprintf(buf, sizeof(buf),(const char*) fmt,ap);
    va_end(ap);
    print_string(fb,buf);
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

    uint32_t used_percent = ((used_pages * 100) / total_pages);
    unsigned char* used_str = large_bytenum_to_decimal_str(used_pages * 0x1000);
    unsigned char* total_str = large_bytenum_to_decimal_str(total_pages * 0x1000);

    print_stringf(fb,"Memory: %s/%s (%d%)",used_str,total_str,used_percent);

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
    uint32_t disk_used_percent = ((used_sectors * 100) / total_sectors);

    print_stringf(fb,"Disk: %s/%s (%d%)",used_str,total_str,disk_used_percent);

    free(used_str);
    free(total_str);
    close(diskinfo);
}
void print_cpuinfo(user_fb_t* fb) {
    unsigned char buf[128] = {0};
    int cpuinfo = open("sysinfo/cpuinfo",FILE_FLAG_NONE);
    int bytes_read = read(cpuinfo,buf,sizeof(buf));
    print_stringf(fb,"CPU: %s",buf);

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
        print_stringf(fb,"%dh ",hours);
    }
    if (minutes > 0) {
        print_stringf(fb,"%dm ",minutes);
    }
    print_stringf(fb,"%ds ",seconds);
    close(uptime);
}

void print_datetime(user_fb_t* fb){
    uint64_t timestamp = gettimeofday();
    date_t date;
    parse_unix_timestamp(timestamp,&date);
    char* months[] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};

    print_stringf(fb,"%s %d, %d",months[date.month - 1],date.day,date.year);

    print_string(fb," ");
    if (date.hour < 10) print_string(fb,"0");
    print_stringf(fb,"%d:",date.hour);
    if (date.minute < 10) print_string(fb,"0");
    print_stringf(fb,"%d",date.minute);
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