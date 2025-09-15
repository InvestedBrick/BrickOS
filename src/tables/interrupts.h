
#ifndef INCLUDE_IDT_H
#define INCLUDE_IDT_H
#include <stdint.h>
#define STANDARD_KERNEL_ATTRIBUTES 0x8e // Present, DPL=0, 32-bit interrupt gate 
#define STANDARD_USER_ATTRIBUTES 0xee // Present, DPL = 3, 32-bit interrupt gate

#define IDT_MAX_ENTRIES 256

#define PIC1 0x20
#define PIC2 0xa0

#define PIC1_START_INTERRUPT 0x20
#define PIC2_START_INTERRUPT (PIC1_START_INTERRUPT + 0x8)

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

#define INT_TIMER PIC1_START_INTERRUPT
#define INT_KEYBOARD (PIC1_START_INTERRUPT + 1)
#define INT_PIC2 (PIC1_START_INTERRUPT + 2)
#define INT_COM2 (PIC1_START_INTERRUPT + 3)
#define INT_COM1 (PIC1_START_INTERRUPT + 4)
#define INT_LPT2 (PIC1_START_INTERRUPT + 5)
#define INT_FLOPPY (PIC1_START_INTERRUPT + 6)
#define INT_LPT1 (PIC1_START_INTERRUPT + 7)
#define INT_CMOS_RTC PIC2_START_INTERRUPT
#define INT_MOUSE (PIC2_START_INTERRUPT + 4)
#define INT_SOFTWARE 0x30

#define INT_PAGE_FAULT 0xe
typedef struct
{
    uint16_t offset_low;
    uint16_t segment_selector;
    uint8_t reserved;
    uint8_t attributes;
    uint16_t offset_high;
}__attribute__((packed)) idt_entry_t;

typedef struct
{
    uint16_t limit;
    uint32_t base;
}__attribute__((packed)) idt_t;


typedef struct {
    uint32_t gs,fs,es,ds;
    uint32_t ebp,edi,esi,edx,ecx,ebx,eax;
    uint32_t interrupt_number,error_code,eip,cs,eflags,esp,ss;
}__attribute__((packed)) interrupt_stack_frame_t;

extern uint32_t ticks;

/**
 * set_idt_entry:
 * creates an idt entry with the given parameters
 * @param num Index of the entry
 * @param offset The interrupt jump address
 * @param attributes The relevant attribute bits
 * @param segment_selector The segment selector
 */
void set_idt_entry(uint8_t num,void* offset,uint8_t attributes);

/**
 * 
 * Sets up the Interrupt Descriptor Table
 */
void init_idt();

void interrupt_handler(interrupt_stack_frame_t* stack_frame);

void load_idt(const idt_t* first_entry);

/**
 * remap_PIC:
 * Remaps the standard PIC1 and PIC2 interrupts so that they dont conflict with BIOS interrupts
 * PIC1 becomes offset1..offset1+7
 * PIC2 becomes offset2..offset2+7
 */
void remap_PIC(uint32_t offset1, uint32_t offset2);

/**
 * acknowledge_PIC:
 * Acknowledges and interrupt from either PIC 1 or PIC 2
 *  
 * @param interrupt The number of the interrupt
 */
void acknowledge_PIC(uint32_t interrupt);


void enable_interrupts();

void disable_interrupts();

/**
 * get_interrupt_status:
 * @return whether interrupts are enabled (return val 1) or not (return val 0)
 */
uint8_t get_interrupt_status();

/**
 * set_interrupt_status:
 * Enbales or disables interrupts based on whether int_enable > 0
 * @param int_enable A boolean to determine if interrupts should be enabled or not
 */
void set_interrupt_status(uint8_t int_enable);


/**
 * setup_timer_switch:
 * Forces the next timer interrupt to switch tasks
 */
void setup_timer_switch();
#endif