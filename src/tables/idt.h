#pragma once

#ifndef INCLUDE_IDT_H
#define INCLUDE_IDT_H

#define STANDARD_KERNEL_ATTRIBUTES 0x8e // Present, DPL=0, 32-bit interrupt gate

#define IDT_MAX_ENTRIES 256
typedef struct
{
    unsigned short offset_low;
    unsigned short segment_selector;
    unsigned char reserved;
    unsigned char attributes;
    unsigned short offset_high;
}__attribute__((packed)) idt_entry_t;

typedef struct
{
    unsigned short limit;
    unsigned int base;
}__attribute__((packed)) idt_t;


typedef struct {
    unsigned int eax;
    unsigned int ebx;
    unsigned int ecx;
    unsigned int edx;
    unsigned int esi;
    unsigned int edi;
    unsigned int ebp;
    unsigned int esp;
}__attribute__((packed)) cpu_state_t;

typedef struct  {
    unsigned int error_code;
    unsigned int eip;
    unsigned int cs;
    unsigned int eflags;
}__attribute__((packed)) stack_state_t;

/**
 * set_idt_entry:
 * creates an idt entry with the given parameters
 * @param num Index of the entry
 * @param offset The interrupt jump address
 * @param attributes The relevant attribute bits
 * @param segment_selector The segment selector
 */
void set_idt_entry(unsigned char num,void* offset,unsigned char attributes);

/**
 * 
 * Sets up the Interrupt Descriptor Table
 */
void init_idt();

void interrupt_handler(cpu_state_t cpu, stack_state_t stack, unsigned int interrupt);

void load_idt(const idt_entry_t* first_entry);
#endif