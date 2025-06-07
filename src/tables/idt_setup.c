#include "idt.h"
#include "../io/log.h"

__attribute__((aligned(0x10)))
static idt_entry_t idt_entries[IDT_MAX_ENTRIES];
static int enabled_idt[IDT_MAX_ENTRIES] = {0};

extern void* idt_code_table[];

static idt_t idt;

void set_idt_entry(unsigned char num,void* offset,unsigned char attributes){
    idt_entries[num].offset_low = ((unsigned int)offset & 0xffff);
    idt_entries[num].segment_selector = 0x08; // Kernel code segment from the gdt
    idt_entries[num].reserved = 0x0;
    idt_entries[num].attributes = attributes;
    idt_entries[num].offset_high = ((unsigned int)offset >> 16) & 0xffff;
}

void init_idt(){
    idt.base = (unsigned int)&idt_entries[0];
    idt.limit = sizeof(idt_entry_t) * IDT_MAX_ENTRIES - 1;
    for(unsigned char v = 0; v < 32;v++){
        set_idt_entry(v,idt_code_table[v],STANDARD_KERNEL_ATTRIBUTES);
        enabled_idt[v] = 1;
    }

    load_idt((idt_entry_t*)&idt);

}

void interrupt_handler(cpu_state_t cpu, stack_state_t stack, unsigned int interrupt) {
    __asm__ volatile ("hlt");
}