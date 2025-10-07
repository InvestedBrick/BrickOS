#include "gdt.h"
#include "tss.h"
#include "../util.h"
tss_entry_t tss_entry;

gdt_entry_t gdt_entries[N_GDT_ENTRIES];
gdt_t gdtp;
void set_gdt_entry(int num,uint32_t base, uint32_t limit, uint8_t access, uint8_t gran){
    gdt_entries[num].base_low = (base & 0xffff);
    gdt_entries[num].base_middle = (base >> 16) & 0xff;
    gdt_entries[num].base_high = (base >> 24) & 0xff;
    gdt_entries[num].limit_low = (limit & 0xffff);
    gdt_entries[num].granularity = ((limit >> 16) & 0x0f) | (gran & 0xf0);
    gdt_entries[num].access = access;
}

void init_gdt() {
    gdtp.size = (sizeof(gdt_entry_t) * N_GDT_ENTRIES ) - 1;
    gdtp.address = (uint32_t) &gdt_entries;
    //        P DPL S E DC RW A
    // 0x9a = 1 00  1 1 0  1  0
    // 0x92 = 1 00  1 0 0  1  0

    //USR EX  1 11  1 1 0  1  0
    //USR DT  1 11  1 0 0  1  0
    set_gdt_entry(0,0,0,0,0); // null segment 0x0
    set_gdt_entry(1,0,0xffffffff,KERNEL_CODE_SEGMENT,FLAGS); // Code segment 0x08
    set_gdt_entry(2,0,0xffffffff,KERNEL_DATA_SEGMENT,FLAGS); // Data segment 0x10

    set_gdt_entry(3,0,0xffffffff,USER_CODE_SEGMENT,FLAGS); // 0x18
    set_gdt_entry(4,0,0xffffffff,USER_DATA_SEGMENT,FLAGS); // 0x20

    write_tss(0x10,0); // 0x10 is the Kernel data segment

    load_gdt((gdt_t*)&gdtp);
    load_tss(0x28);
}

void write_tss(uint16_t ss0, uint32_t esp0){
    set_gdt_entry(5,(uint32_t)&tss_entry,sizeof(tss_entry_t) - 1,0x89,FLAGS  & ~(0x80)); // 0x89 = present, ring 0, type 9 (TSS)
    memset(&tss_entry,0,sizeof(tss_entry_t));
    tss_entry.ss0 = ss0;
    tss_entry.esp0 = esp0;
    tss_entry.iomap_base = sizeof(tss_entry_t);
}

void set_kernel_stack(uint32_t stack){
    tss_entry.esp0 = stack;
}
