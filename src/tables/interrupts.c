#include "interrupts.h"
#include "../io/log.h"
#include "../drivers/keyboard/keyboard.h"
#include "../processes/scheduler.h"
#include "../memory/memory.h"
#include "syscalls.h"
#include "../filesystem/file_operations.h"
static idt_entry_t idt_entries[IDT_MAX_ENTRIES] __attribute__((aligned(0x10)));
static int enabled_idt[IDT_MAX_ENTRIES] = {0};

extern void* idt_code_table[];

static idt_t idt;

unsigned int ticks = 0;
unsigned char interrupts_enabled = 1;
void enable_interrupts(){
    asm volatile ("sti");
    interrupts_enabled = 1;
}

void disable_interrupts(){
    asm volatile ("cli");
    interrupts_enabled = 0;
}

unsigned char get_interrupt_status(){
    return interrupts_enabled;
}

void set_interrupt_status(unsigned char int_enable){
    if (int_enable){
        enable_interrupts();
    }else{
        disable_interrupts();
    }
}
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
    for(unsigned char v = 0; v < 49;v++){
        if(v == INT_SOFTWARE){
            set_idt_entry(v,idt_code_table[v],STANDARD_USER_ATTRIBUTES); // user software interrupt call (syscall)
        }else{
            set_idt_entry(v,idt_code_table[v],STANDARD_KERNEL_ATTRIBUTES);
        }
        enabled_idt[v] = 1;
    }
    load_idt(&idt);

    remap_PIC(PIC1_START_INTERRUPT,PIC2_START_INTERRUPT); // [32; 39] and [40;47]

}

void handle_software_interrupt(interrupt_stack_frame_t* stack_frame){
    switch (stack_frame->eax)
    {
    case 0x1:
        {
            setup_kernel_process(stack_frame);
            break;
        }
    case SYS_WRITE:
        {
            write(stack_frame->ebx,(unsigned char*)stack_frame->ecx,stack_frame->edx) ;
            break;  
        }
    default:
        break;
    }

}


void interrupt_handler(interrupt_stack_frame_t* stack_frame) {
    interrupts_enabled = 0; // to stop other functions from copying a wrong value

    if (stack_frame->interrupt_number == INT_KEYBOARD){
        handle_keyboard_interrupt();
        acknowledge_PIC(stack_frame->interrupt_number);
    }
    else if (stack_frame->interrupt_number == INT_TIMER){
        ticks++;
        if (ticks % TASK_SWITCH_TICKS == 0) {
            switch_task(stack_frame);
        }

        acknowledge_PIC(stack_frame->interrupt_number);
    }
    else if(stack_frame->interrupt_number == INT_SOFTWARE){
        handle_software_interrupt(stack_frame);
    }

    // if we return to kernel -> esp and ss not needed
    if ((stack_frame->cs & 0x3) == 0) stack_frame->error_code = RETURN_SMALLER_STACK;

    interrupts_enabled = 1; // the interrupts only actually get enabled in the iret
}

void remap_PIC(unsigned int offset1, unsigned int offset2){
    outb(PIC1_COMMAND, ICW4_INIT | ICW1_ICW4); // starts the remapping sequence in cascade mode
    outb(PIC2_COMMAND, ICW4_INIT | ICW1_ICW4);

    outb(PIC1_DATA,offset1); // set new offsets
    outb(PIC2_DATA,offset2);
   
    outb(PIC1_DATA, 0x4); //slavery
    outb(PIC2_DATA,0x2);

    outb(PIC1_DATA,ICW4_8086); //use 8086 mode
    outb(PIC2_DATA,ICW4_8086);

    //unmask
    outb(PIC1_DATA,0x0);
    outb(PIC2_DATA,0x0);

}

void acknowledge_PIC(unsigned int interrupt){
    if (interrupt < PIC1_START_INTERRUPT || interrupt > PIC2_START_INTERRUPT + 7){
        return;
    }

    if(interrupt < PIC2_START_INTERRUPT){
        outb(PIC1,PIC_EOI);
    }else{
        outb(PIC2,PIC_EOI);
    }
}