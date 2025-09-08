#include "interrupts.h"
#include "../io/log.h"
#include "../drivers/keyboard/keyboard.h"
#include "../processes/scheduler.h"
#include "../memory/memory.h"
#include "syscalls.h"
#include "syscall_defines.h"
#include "../filesystem/file_operations.h"
#include "../filesystem/filesystem.h"
#include "../memory/kmalloc.h"
static idt_entry_t idt_entries[IDT_MAX_ENTRIES] __attribute__((aligned(0x10)));
static int enabled_idt[IDT_MAX_ENTRIES] = {0};

extern void* idt_code_table[];

static idt_t idt;

uint32_t ticks = 0;
uint8_t interrupts_enabled = 1;
uint8_t force_switch = 0;

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

uint8_t get_interrupt_status(){
    return interrupts_enabled;
}

void set_interrupt_status(uint8_t int_enable){
    if (int_enable){
        enable_interrupts();
    }else{
        disable_interrupts();
    }
}
void set_idt_entry(uint8_t num,void* offset,uint8_t attributes){
    idt_entries[num].offset_low = ((uint32_t)offset & 0xffff);
    idt_entries[num].segment_selector = 0x08; // Kernel code segment from the gdt
    idt_entries[num].reserved = 0x0;
    idt_entries[num].attributes = attributes;
    idt_entries[num].offset_high = ((uint32_t)offset >> 16) & 0xffff;
}

void init_idt(){
    idt.base = (uint32_t)&idt_entries[0];
    idt.limit = sizeof(idt_entry_t) * IDT_MAX_ENTRIES - 1;
    for(uint8_t v = 0; v < 49;v++){
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
    case SYS_DEBUG:
        {
            //temporary
            log((unsigned char*)stack_frame->ebx);
            return 0;
        }
    case SYS_EXIT: 
        return sys_exit(get_current_user_process(),stack_frame);
    case SYS_OPEN:
        return sys_open(get_current_user_process(),(unsigned char*)stack_frame->ebx,(uint8_t)stack_frame->ecx);
    case SYS_CLOSE:
        return sys_close(get_current_user_process(),stack_frame->ebx);
    case SYS_READ:
        return sys_read(get_current_user_process(),stack_frame->ebx,(unsigned char*)stack_frame->ecx,stack_frame->edx);
    case SYS_WRITE:
        return sys_write(get_current_user_process(),stack_frame->ebx,(unsigned char*)stack_frame->ecx,stack_frame->edx);
    case SYS_SEEK:
        return sys_seek(get_current_user_process(),stack_frame->ebx,stack_frame->ecx);
    case SYS_ALLOC_PAGE:
        return sys_mmap(get_current_user_process(),0,stack_frame->ebx,stack_frame->ecx,stack_frame->edx,stack_frame->edi,stack_frame->esi);
    case SYS_GETCWD:
        return sys_getcwd((unsigned char*)stack_frame->ebx, stack_frame->ecx);
    case SYS_GETDENTS:
        return sys_getdents(get_current_user_process(),stack_frame->ebx,(dirent_t*)stack_frame->ecx,stack_frame->edx);
    case SYS_CHDIR:
        return sys_chdir((unsigned char*)stack_frame->ebx);
    case SYS_RMFILE:
        return sys_rmfile((unsigned char*)stack_frame->ebx);
    case SYS_MKNOD:
        return sys_mknod((unsigned char*)stack_frame->ebx,(mknod_params_t*)stack_frame->ecx);
    case SYS_IOCTL:
        return sys_ioctl(get_current_user_process(),stack_frame->ebx,stack_frame->ecx,(void*)stack_frame->edx);
    case SYS_MSSLEEP:
        return sys_mssleep(stack_frame,stack_frame->ebx);
    case SYS_SPAWN:
        return sys_spawn((unsigned char*)stack_frame->ebx,(unsigned char**)stack_frame->ecx,(process_fds_init_t*)stack_frame->edx);
    case SYS_GETPID:
        return sys_getpid(get_current_user_process());    
    default:
        break;
    }

    return 0;
}

uint32_t init_new_page(virt_mem_area_t* vma,user_process_t* p,uint32_t aligned_fault_addr){
    uint32_t frame = pmm_alloc_page_frame();        
    mem_map_page(TEMP_KERNEL_COPY_ADDR,frame,PAGE_FLAG_WRITE | PAGE_FLAG_PRESENT);
    
    if (vma->fd != MAP_FD_NONE){
        uint32_t file_off = vma->offset + (aligned_fault_addr - (uint32_t)vma->addr);
        sys_seek(p,vma->fd,file_off);
        sys_read(p,vma->fd,(unsigned char*)TEMP_KERNEL_COPY_ADDR,MEMORY_PAGE_SIZE);
    }else{
        memset((void*)TEMP_KERNEL_COPY_ADDR,0,MEMORY_PAGE_SIZE);
    }
    
    mem_unmap_page(TEMP_KERNEL_COPY_ADDR);

    return frame;
}

void page_fault_handler(user_process_t* p,uint32_t fault_addr,interrupt_stack_frame_t* stack_frame){

    virt_mem_area_t* vma = find_virt_mem_area(p->vm_areas,fault_addr);
    if (!vma){
        error("A page fault has occured");
        error("process:");
        error(p->process_name);
        log_uint(fault_addr);
        error("Cause:");
        if (stack_frame->error_code & 0x1) error("Access of non-present page");
        if (stack_frame->error_code & 0x2) error("Write access"); else error("Read access");
        if (stack_frame->error_code & 0x4) error("User");
        if (stack_frame->error_code & 0x8) error("Reserved Write");
        if (stack_frame->error_code & 0x10) error("Non-Executable instruction fetch");
        if (stack_frame->error_code & 0x20) error("Protection key violation");

        stack_frame->ebx = 1; // exit code
        sys_exit(p,stack_frame);
    }

    uint32_t aligned_fault_addr = ALIGN_DOWN(fault_addr,MEMORY_PAGE_SIZE);

    uint32_t frame;

    if (vma->shrd_obj){
        
        int page_idx = ((aligned_fault_addr - (uint32_t)vma->addr) + vma->offset) / MEMORY_PAGE_SIZE;

        if (!vma->shrd_obj->shared_pages[page_idx]){
            frame = init_new_page(vma,p,aligned_fault_addr);

            vma->shrd_obj->shared_pages[page_idx] = (shared_page_t*)kmalloc(sizeof(shared_page_t));
            vma->shrd_obj->shared_pages[page_idx]->phys_addr = frame;
            vma->shrd_obj->shared_pages[page_idx]->ref_count = 1;
        }else{
            frame = vma->shrd_obj->shared_pages[page_idx]->phys_addr;
            vma->shrd_obj->shared_pages[page_idx]->ref_count++;
        }

    }else{
        frame = init_new_page(vma,p,aligned_fault_addr);
    }

    int page_flags = PAGE_FLAG_USER;
    if (vma->prot & PROT_WRITE) page_flags |= PAGE_FLAG_WRITE;

    mem_map_page(aligned_fault_addr,frame,page_flags);
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

            manage_sleeping_processes();

            if (ticks % TASK_SWITCH_TICKS == 0 || force_switch) {
                if (force_switch) force_switch = 0;
                switch_task(stack_frame);
            }
            acknowledge_PIC(stack_frame->interrupt_number);
            break;
        case INT_SOFTWARE: {
            stack_frame->eax = handle_software_interrupt(stack_frame);
            break;
        }
        case INT_PAGE_FAULT:{

            uint32_t cr2;
            asm volatile ("mov %%cr2, %0" : "=r"(cr2));
            page_fault_handler(get_current_user_process(),cr2,stack_frame);
            break;
        }
        default:
            break;
    }

    interrupts_enabled = 1; // the interrupts only actually get enabled in the iret
}

void remap_PIC(uint32_t offset1, uint32_t offset2){
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

void acknowledge_PIC(uint32_t interrupt){
    if (interrupt < PIC1_START_INTERRUPT || interrupt > PIC2_START_INTERRUPT + 7){
        return;
    }

    if(interrupt < PIC2_START_INTERRUPT){
        outb(PIC1,PIC_EOI);
    }else{
        outb(PIC2,PIC_EOI);
    }
}