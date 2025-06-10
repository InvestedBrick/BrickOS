#include "gdt.h"

gdt_entry_t gdt_entries[N_GDT_ENTRIES];
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
    gdtp.size = (sizeof(gdt_entry_t) * N_GDT_ENTRIES ) - 1;
    gdtp.address = (unsigned int) &gdt_entries;
    //        P DPL S E DC RW A
    // 0x9a = 1 00  1 1 0  1  0
    // 0x92 = 1 00  1 0 0  1  0

    //USR EX  1 11  1 1 0  1  0
    //USR DT  1 11  1 0 0  1  0
    set_gdt_entry(0,0,0,0,0); // null segment
    set_gdt_entry(1,0,0xffffffff,KERNEL_CODE_SEGMENT,GRANULARITY); // Code segment
    set_gdt_entry(2,0,0xffffffff,KERNEL_DATA_SEGMENT,GRANULARITY); // Data segment

    set_gdt_entry(3,0,0xffffffff,USER_CODE_SEGMENT,GRANULARITY);
    set_gdt_entry(4,0,0xffffffff,USER_DATA_SEGNENT,GRANULARITY);

    load_gdt((gdt_t*)&gdtp);
}