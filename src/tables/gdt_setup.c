#include "gdt.h"

gdt_entry_t gdt_entries[3];
gdt_t gdtp;
void set_gdt_entry(int num,unsigned int base, unsigned int limit, unsigned char access, unsigned char gran){
    gdt_entries[num].base_low = (base & 0xffff);
    gdt_entries[num].base_middle = (base >> 16) & 0xff;
    gdt_entries[num].base_high = (base >> 24) & 0xff;
    gdt_entries[num].limit_low = (limit & 0xffff);
    gdt_entries[num].granularity = ((limit >> 16) & 0x0f) | (gran & 0xf0);
    gdt_entries[num].access = access;
}

void init_gdt() {
    gdtp.size = (sizeof(gdt_entry_t) * 3 ) - 1;
    gdtp.address = (unsigned int) &gdt_entries;

    set_gdt_entry(0,0,0,0,0); // null segment
    set_gdt_entry(1,0,0xffffffff,0x9a,0xcf); // Code segment
    set_gdt_entry(2,0,0xffffffff,0x92,0xcf); // Data segment

    load_gdt((gdt_t*)&gdtp);
}