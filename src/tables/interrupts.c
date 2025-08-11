#include "interrupts.h"
#include "../io/log.h"
#include "../drivers/keyboard/keyboard.h"
#include "../processes/scheduler.h"
#include "../memory/memory.h"
#include "syscalls.h"
#include "syscall_defines.h"
#include "../filesystem/file_operations.h"
static idt_entry_t idt_entries[IDT_MAX_ENTRIES] __attribute__((aligned(0x10)));
static int enabled_idt[IDT_MAX_ENTRIES] = {0};

extern void* idt_code_table[];

static idt_t idt;

unsigned int ticks = 0;
unsigned char interrupts_enabled = 1;
unsigned char force_switch = 0;

void setup_timer_switch(){
    force_switch = 1;
}

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

int handle_software_interrupt(interrupt_stack_frame_t* stack_frame){
    switch (stack_frame->eax)
    {
    case SYS_EXIT: 
        return sys_exit(get_current_user_process(),stack_frame);
    case SYS_OPEN:
        return sys_open(get_current_user_process(),(unsigned char*)stack_frame->ebx,(unsigned char)stack_frame->ecx);
    case SYS_CLOSE:
        return sys_close(get_current_user_process(),stack_frame->ebx);
    case SYS_READ:
        return sys_read(get_current_user_process(),stack_frame->ebx,(unsigned char*)stack_frame->ecx,stack_frame->edx);
    case SYS_WRITE:
        return sys_write(get_current_user_process(),stack_frame->ebx,(unsigned char*)stack_frame->ecx,stack_frame->edx);
    case SYS_ALLOC_PAGE:
        return sys_mmap(get_current_user_process(),stack_frame->ebx);
    case SYS_GETCWD:
        return sys_getcwd((unsigned char*)stack_frame->ebx, stack_frame->ecx);
    default:
        break;
    }

    return 0;
}


void interrupt_handler(interrupt_stack_frame_t* stack_frame) {
    interrupts_enabled = 0; // to stop other functions from copying a wrong value

    switch (stack_frame->interrupt_number) {
        case INT_KEYBOARD:
            handle_keyboard_interrupt();
            acknowledge_PIC(stack_frame->interrupt_number);
            break;
        case INT_TIMER:
            ticks++;
            if (ticks % TASK_SWITCH_TICKS == 0 || force_switch) {
                if (force_switch) force_switch = 0;
                switch_task(stack_frame);
            }
            acknowledge_PIC(stack_frame->interrupt_number);
            break;
        case INT_SOFTWARE: {
            int ret_val = handle_software_interrupt(stack_frame);
            if (ret_val != 0){
                stack_frame->eax = ret_val;
            }
            break;
        }
        case INT_PAGE_FAULT:{
            error("A page fault has occured");
            unsigned int cr2;
            asm volatile ("mov %%cr2, %0" : "=r"(cr2));
            log_uint(cr2);
            error("Cause:");
            if (stack_frame->error_code & 0x1) error("Access of non-present page");
            if (stack_frame->error_code & 0x2) error("Write access"); else error("Read access");
            if (stack_frame->error_code & 0x4) error("User");
            if (stack_frame->error_code & 0x8) error("Reserved Write");
            if (stack_frame->error_code & 0x10) error("Non-Executable instruction fetch");
            if (stack_frame->error_code & 0x20) error("Protection key violation");
            break;
        }
        default:
            break;
    }

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