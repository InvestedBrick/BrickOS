#include "gdt.h"
#include "tss.h"
#include "../util.h"
tss_entry_t tss_entry;

gdt_entry_t gdt_entries[N_GDT_ENTRIES];
gdt_t gdtp;
void set_gdt_entry(int num,uint32_t base, uint32_t limit, uint8_t access, uint8_t flags){
    gdt_entries[num].base_low    = (base & 0xffff);
    gdt_entries[num].base_middle = (base >> 16) & 0xff;
    gdt_entries[num].base_high   = (base >> 24) & 0xff;
    gdt_entries[num].limit_low   = (limit & 0xffff);
    gdt_entries[num].flags       = ((limit >> 16) & 0x0f) | (flags & 0xf0);
    gdt_entries[num].access      = access;
}

// remeber to reserve 2 gdt entries for this one
void set_gdt_entry64(int num, uint64_t base,uint32_t limit, uint8_t access, uint8_t flags){
    gdt_entry64_t* entry = (gdt_entry64_t*)&gdt_entries[num];

    entry->base_low   = base & 0xffff;
    entry->base_mid   = (base >> 16) & 0xff;
    entry->base_high  = (base >> 24) & 0xff;
    entry->base_upper = (base >> 32) & 0xffffffff;
    entry->access     = access;
    entry->limit_low  = limit & 0xffff;
    entry->flags      = ((limit >> 16) & 0x0f) | (flags & 0xf0);
    entry->reserved   = 0x0;
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
    set_gdt_entry(1,0,0xffff,KERNEL_CODE_SEGMENT_ACCESS,GDT_CODE_SEG_FLAGS); // Code segment 0x08
    set_gdt_entry(2,0,0xffff,KERNEL_DATA_SEGMENT_ACCESS,GDT_DATA_SEG_GLAGS); // Data segment 0x10

    set_gdt_entry(3,0,0xffff,USER_CODE_SEGMENT_ACCESS,GDT_CODE_SEG_FLAGS); // 0x18
    set_gdt_entry(4,0,0xffff,USER_DATA_SEGMENT_ACCESS,GDT_DATA_SEG_GLAGS); // 0x20

    write_tss(0x0); 

    load_gdt((gdt_t*)&gdtp);
    load_tss(0x28);
}

void write_tss(uint64_t rsp0){
    memset(&tss_entry,0,sizeof(tss_entry_t));
    set_gdt_entry64(5,(uint64_t)&tss_entry,sizeof(tss_entry_t) - 1,TSS_ACCESS,TSS_FLAGS); // 0x89 = present, ring 0, type 9 (TSS)
    tss_entry.rsp0 = rsp0;
    tss_entry.iobp = sizeof(tss_entry_t);
}

void set_kernel_stack(uint64_t stack){
    tss_entry.rsp0 = stack;
}
