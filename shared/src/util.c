#include <shared/util.h>
#include <shared/format.h>
#include <stddef.h>

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

uint8_t strneq(const unsigned char* str1, const unsigned char* str2, uint32_t len_1, uint32_t len_2){
    uint32_t max_len = len_1 > len_2 ? len_1 : len_2;

    for (uint32_t i = 0; i < max_len;i++){
        if (str1[i] != str2[i]) return 0;

        if (str1[i] == '\0' && str2[i] == '\0') return 1;
    }
    return 1;
}

uint32_t rfind_char(unsigned char* str, unsigned char c){
    uint32_t str_len = strlen(str);

    for (int i = str_len - 1; i >= 0; i--){
        if (str[i] == c) return (uint32_t)i;
    }

    return (uint32_t)-1;
}

int memcmp(const void* ptr1, const void* ptr2, size_t num) {
    const unsigned char* p1 = ptr1;
    const unsigned char* p2 = ptr2;

    for (size_t i = 0; i < num; i++) {
        if (p1[i] != p2[i]) {
            return p1[i] - p2[i];
        }
    }
    return 0;
}

uint64_t min(uint64_t a, uint64_t b){
    return a < b ? a : b;
}

uint64_t max(int64_t a, int64_t b){
    return a > b ? a : b;
}

void ipv4_to_str(uint32_t ip_addr, unsigned char* out_buffer){
    write_bufferf(out_buffer,16,"%d.%d.%d.%d",  
        (ip_addr >> 24) & 0xFF,
        (ip_addr >> 16) & 0xFF,
        (ip_addr >> 8) & 0xFF,
        ip_addr & 0xFF
    );
}

uint32_t ipv4_to_uint32(unsigned char* str){
    uint32_t parts[4] = {0};
    uint32_t value = 0;
    int part = 0;

    while (*str){
        if (*str >= '0' && *str <= '9'){
            value = value * 10 + (*str - '0');

            if (value > 255)
                return 0;

            str++;
        }
        else if (*str == '.'){
            if (part >= 3)
                return 0;

            parts[part++] = value;
            value = 0;
            str++;
        }
        else{
            return 0;
        }
    }

    if (part != 3)
        return 0;

    parts[3] = value;

    return (parts[0] << 24) |
           (parts[1] << 16) |
           (parts[2] << 8)  |
            parts[3];
}

void mac_to_str(uint8_t* mac_addr, unsigned char* out_buffer){
    out_buffer[17] = 0;
    char* hex = "0123456789abcdef";
    for (uint32_t i = 0; i < 6;i++){
        uint8_t byte = mac_addr[i];
        out_buffer[i * 3] = hex[byte / 16];
        out_buffer[i * 3 + 1] = hex[byte % 16];
        if (i < 5)
            out_buffer[i * 3 + 2] = ':';
    }

}

uint32_t ascii_to_uint32(unsigned char* str){
    uint32_t result = 0;
    for (uint32_t i = 0; str[i];i++){
        if (str[i] < '0' || str[i] > '9') break;
        result = result * 10 + (str[i] - '0');
    }

    return result;
}