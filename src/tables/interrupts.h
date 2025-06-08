#pragma once

#ifndef INCLUDE_IDT_H
#define INCLUDE_IDT_H

#define STANDARD_KERNEL_ATTRIBUTES 0x8e // Present, DPL=0, 32-bit interrupt gate

#define IDT_MAX_ENTRIES 256

#define PIC1 0x20
#define PIC2 0xa0

#define PIC1_START_INTERRUPT 0x20
#define PIC2_START_INTERRUPT 0x28

#define PIC1_COMMAND PIC1
#define PIC1_DATA   (PIC1 + 1)
#define PIC2_COMMAND PIC2
#define PIC2_DATA   (PIC2 + 1)

#define PIC_EOI 0x20

#define ICW1_ICW4 0x01 // UCW4 will be present
#define ICW1_SINGLE 0x02 // single cascade mode
#define ICW1_INTERVAL4 0x04 // address interval 4
#define ICW4_LEVEL 0x08 // Level triggered (edge) mode
#define ICW4_INIT  0x10 // Initialization
#define ICW4_8086  0x01 // 8086 mode
#define ICW4_AUTO  0x02 // auto EOI (end of interrupt)
#define ICW4_BUF_PIC2 0x08 
#define ICW4_BUF_PIC1 0x0c
#define ICW4_SFMN 0x10  // special fully nested

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

/*
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
*/

typedef struct {
    unsigned int ebp,edi,esi,edx,ecx,ebx,eax;
    unsigned int interrupt_number,error_code,eip,cs,eflags;
}__attribute__((packed)) interrupt_stack_frame_t;

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

void interrupt_handler(interrupt_stack_frame_t* stack_frame);

void load_idt(const idt_entry_t* first_entry);

/**
 * remap_PIC:
 * Remaps the standard PIC1 and PIC2 interrupts so that they dont conflict with BIOS interrupts
 * PIC1 becomes offset1..offset1+7
 * PIC2 becomes offset2..offset2+7
 */
void remap_PIC(unsigned int offset1, unsigned int offset2);

/**
 * acknowledge_PIC:
 * Acknowledges and interrupt from either PIC 1 or PIC 2
 *  
 * @param interrupt The number of the interrupt
 */
void acknowledge_PIC(unsigned int interrupt);
#endif