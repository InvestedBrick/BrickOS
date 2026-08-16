#include "interrupts.h"
#include "../io/log.h"
#include "../drivers/PS2/keyboard/keyboard.h"
#include "../drivers/PS2/mouse/mouse.h"
#include "../drivers/PS2/ps2_controller.h"
#include "../processes/scheduler.h"
#include "../memory/memory.h"
#include "syscalls.h"
#include "syscall_defines.h"
#include "../filesystem/file_operations.h"
#include "../filesystem/filesystem.h"
#include "../memory/kmalloc.h"
#include "../ACPI/apic.h"
#include "../drivers/timer/pit.h"
#include "../kernel_header.h"
#include "timer_callbacks.h"
#include "../networking/network_defines.h"

void page_fault_handler(process_t* p,uint64_t fault_addr,interrupt_stack_frame_t* stack_frame);

interrupt_handler_t* int_head;
static idt_entry_t idt_entries[IDT_MAX_ENTRIES] __attribute__((aligned(0x10)));
static int enabled_idt[IDT_MAX_ENTRIES] = {0};

extern void* idt_code_table[];

static idt_t idt;

uint64_t ticks = 0;
uint64_t current_timestamp = 0;
int timezone_adjustment;
static volatile uint8_t interrupts_enabled = 1;
static volatile uint8_t force_switch = 0;
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

interrupt_handler_t* register_irq(uint32_t int_num, interrupt_function_ptr int_handler){
    interrupt_handler_t* head = int_head;
    interrupt_handler_t* handler = (interrupt_handler_t*)kmalloc(sizeof(interrupt_handler_t));
    
    handler->next = nullptr;
    handler->int_num = int_num;
    handler->handler = int_handler;
    handler->special_arg = nullptr;

    if (!head) int_head = handler;
    else{
        while(head->next) head = head->next;
        head->next = handler;
    }

    return handler;

}

void unregister_irq(uint32_t int_num){
    if (!int_head) return;

    interrupt_handler_t* head = int_head;
    while(head->next && head->next->int_num != int_num) head = head->next;

    if (!head->next) return; // irq does not exist

    interrupt_handler_t* to_delete = head->next;
    head->next = head->next->next;
    kfree(to_delete);
}

void timer_stub(interrupt_stack_frame_t* stack_frame){
    ticks++;
    current_timestamp = limine_data.boot_time + CEIL_DIV((ticks << 32),timer_freq) + timezone_adjustment;
    manage_sleeping_threads();
    handle_timer_callbacks();
    if (ticks % TASK_SWITCH_TICKS == 0 || force_switch) {
        if (force_switch) force_switch = 0;
        switch_task(stack_frame);
    }
}

void page_fault_stub(interrupt_stack_frame_t* stack_frame){
    uint64_t cr2;
    asm volatile ("mov %%cr2, %0" : "=r"(cr2));
    thread_t* curr_thread = get_current_thread();
    if (cr2 == MAGIC_SCHED_FAULT_ADDR && curr_thread->expect_sched_fault){
        curr_thread->expect_sched_fault = false;
        stack_frame->rip = (uint64_t)sched_fault_fixup; // return label after the drop to here
        switch_task(stack_frame); // does not return
    }
    page_fault_handler(get_current_process(),cr2,stack_frame);
}

void set_idt_entry(uint8_t num,uint64_t offset,uint8_t attributes){
    idt_entries[num].offset_low = (offset & 0xffff);
    idt_entries[num].segment_selector = 0x08; // Kernel code segment from the gdt
    idt_entries[num].reserved = 0x0;
    idt_entries[num].ist = 0x0;
    idt_entries[num].attributes = attributes;
    idt_entries[num].offset_mid = (offset >> 16) & 0xffff;
    idt_entries[num].offset_high = (offset >> 32) & 0xffffffff;
}


void handle_software_interrupt(interrupt_stack_frame_t* stack_frame){
    uint64_t rax = 0;
    process_t* p = get_current_process();
    switch (stack_frame->rax)
    {
    case SYS_DEBUG:
        //temporary
        log((unsigned char*)stack_frame->rdi);
        break;
    case SYS_EXIT: 
        rax =  sys_exit(p,stack_frame);
        break;
    case SYS_OPEN:
        rax =  sys_open(p,(unsigned char*)stack_frame->rdi,(uint8_t)stack_frame->rsi);
        break;
    case SYS_CLOSE:
        rax =  sys_close(p,stack_frame->rdi);
        break;
    case SYS_READ:
        rax =  sys_read(p,stack_frame->rdi,(unsigned char*)stack_frame->rsi,stack_frame->rdx);
        break;
    case SYS_WRITE:
        rax =  sys_write(p,stack_frame->rdi,(unsigned char*)stack_frame->rsi,stack_frame->rdx);
        break;
    case SYS_SEEK:
        rax =  sys_seek(p,stack_frame->rdi,stack_frame->rsi,stack_frame->rdx);
        break;
    case SYS_MMAP:
        rax =  sys_mmap(p,MMAP_UNSPEC_ADDR,stack_frame->rdi,stack_frame->rsi,stack_frame->rdx,stack_frame->r10,stack_frame->r8);
        break;
    case SYS_MUNMAP:
        rax = sys_munmap(p,stack_frame->rdi,stack_frame->rsi);
        break;
    case SYS_GETCWD:
        rax =  sys_getcwd((unsigned char*)stack_frame->rdi, stack_frame->rsi);
        break;
    case SYS_GETDENTS:
        rax =  sys_getdents(p,stack_frame->rdi,(dirent_t*)stack_frame->rsi,stack_frame->rdx);
        break;
    case SYS_CHDIR:
        rax =  sys_chdir((unsigned char*)stack_frame->rdi);
        break;
    case SYS_RMFILE:
        rax =  sys_rmfile((unsigned char*)stack_frame->rdi);
        break;
    case SYS_MKNOD:
        rax =  sys_mknod((unsigned char*)stack_frame->rdi,(mknod_params_t*)stack_frame->rsi);
        break;
    case SYS_IOCTL:
        rax =  sys_ioctl(p,stack_frame->rdi,stack_frame->rsi,(void*)stack_frame->rdx);
        break;
    case SYS_MSSLEEP:
        rax =  sys_mssleep(stack_frame,stack_frame->rdi);
        break;
    case SYS_SPAWN:
        rax =  sys_spawn((unsigned char*)stack_frame->rdi,(unsigned char**)stack_frame->rsi,(process_fds_init_t*)stack_frame->rdx);
        break;
    case SYS_GETPID:
        rax =  sys_getpid(p); 
        break;   
    case SYS_GETTIMEOFDAY:
        rax =  sys_gettimeofday();
        break;
    case SYS_SETTIMEZONE:
        rax =  sys_settimezone(p,(int)stack_frame->rdi);
        break;
    case SYS_SOCKET:
        rax =  sys_socket(p,stack_frame->rdi,stack_frame->rsi,stack_frame->rdx);
        break;
    case SYS_BIND:
        rax =  sys_bind(p,stack_frame->rdi,(sockaddr_t*)stack_frame->rsi,stack_frame->rdx);
        break;
    case SYS_RECVFROM:
        rax =  sys_recvfrom(p,stack_frame->rdi,(void*)stack_frame->rsi,stack_frame->rdx,stack_frame->r10,(sockaddr_t*)stack_frame->r8,(uint32_t*)stack_frame->r9);
        break;
    case SYS_SENDTO:
        rax =  sys_sendto(p,stack_frame->rdi,(void*)stack_frame->rsi,stack_frame->rdx,stack_frame->r10,(sockaddr_t*)stack_frame->r8,stack_frame->r9);
        break;
    case SYS_SETSOCKOPT:
        rax  = sys_setsockopt(p,stack_frame->rdi,stack_frame->rsi,stack_frame->rdx, (void*)stack_frame->r10,stack_frame->r8);
        break;
    case SYS_GETSOCKOPT:
        rax  = sys_getsockopt(p,stack_frame->rdi,stack_frame->rsi,stack_frame->rdx, (void*)stack_frame->r10,(uint32_t*)stack_frame->r8);
        break;
    default:
        rax = (uint64_t)SYSCALL_FAIL;
        break;
    }

    stack_frame->rax = rax;
}


uint64_t init_new_page(virt_mem_area_t* vma,process_t* p,uint64_t aligned_fault_addr){
    uint64_t frame = pmm_alloc_page_frame();        
    mem_map_page(USER_SCRATCH_PAGE,frame,PAGE_FLAG_WRITE | PAGE_FLAG_USER);
    
    if (vma->fd != MAP_FD_NONE){
        uint64_t file_off = vma->offset + (aligned_fault_addr - vma->addr);
        sys_seek(p,vma->fd,file_off,SEEK_SET);
        sys_read(p,vma->fd,(unsigned char*)USER_SCRATCH_PAGE,MEMORY_PAGE_SIZE);
    }else{
        memset((void*)USER_SCRATCH_PAGE,0,MEMORY_PAGE_SIZE);
    }
    
    mem_unmap_page(USER_SCRATCH_PAGE);

    return frame;
}

void page_fault_handler(process_t* p,uint64_t fault_addr,interrupt_stack_frame_t* stack_frame){
    uint8_t int_status = get_interrupt_status();
    disable_interrupts();
    if (handle_phdr_mapping(p,fault_addr)) {
        set_interrupt_status(int_status);
        return;
    }
    virt_mem_area_t* vma = find_virt_mem_area(p->vm_areas,fault_addr);
    if (!vma){
        errorf("A page fault has occured in process: %s",p->process_name);
        errorf("At address: %x",fault_addr);
        error("Cause:");
        if (stack_frame->error_code & 0x1) error("Access of non-present page");
        if (stack_frame->error_code & 0x2) error("Write access"); else error("Read access");
        if (stack_frame->error_code & 0x4) error("User");
        if (stack_frame->error_code & 0x8) error("Reserved Write");
        if (stack_frame->error_code & 0x10) error("Non-Executable instruction fetch");
        if (stack_frame->error_code & 0x20) error("Protection key violation");

        stack_frame->rdi = 1; // exit code
        set_interrupt_status(int_status);
        sys_exit(p,stack_frame);
    }

    uint64_t aligned_fault_addr = ALIGN_DOWN(fault_addr,MEMORY_PAGE_SIZE);

    uint64_t frame;
    
    if (vma->shrd_obj){
        
        int page_idx = ((aligned_fault_addr - vma->addr) + vma->offset) / MEMORY_PAGE_SIZE;

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

    vector_append(&vma->mapped_pages,(vector_data_t)aligned_fault_addr);

    mem_map_page(aligned_fault_addr,frame,page_flags);
    set_interrupt_status(int_status);
}

void interrupt_handler(interrupt_stack_frame_t* stack_frame) {
    
    disable_interrupts();
    interrupt_handler_t* head = int_head;
    while (head && head->int_num != stack_frame->interrupt_number) head = head->next;
    enable_interrupts();
    
    if (!head) {
        if (stack_frame->interrupt_number < APIC_INTERRUPT_START)logf("Fault occured: %x (%s)",stack_frame->interrupt_number,get_current_process()->process_name);
        return;
    }
    head->handler(head->special_arg ? head->special_arg : stack_frame);
    
    write_apic_register(APIC_REG_EOI,0x0);
}

void init_idt(){
    idt.base = (uint64_t)&idt_entries[0];
    idt.limit = sizeof(idt_entry_t) * IDT_MAX_ENTRIES - 1;
    for(uint8_t v = 0; v < 49;v++){
        if(v == INT_SOFTWARE){
            set_idt_entry(v,(uint64_t)idt_code_table[v],STANDARD_USER_ATTRIBUTES); // user software interrupt call (syscall)
        }else{
            set_idt_entry(v,(uint64_t)idt_code_table[v],STANDARD_KERNEL_ATTRIBUTES);
        }
        enabled_idt[v] = 1;
    }
    load_idt(&idt);

    remap_PIC(PIC1_START_INTERRUPT,PIC2_START_INTERRUPT); // [32; 39] and [40;47]

}

void register_basic_interrupts(){
    register_irq(INT_SOFTWARE,handle_software_interrupt);
    register_irq(INT_PAGE_FAULT,page_fault_stub);

    uint8_t timer_irq = ioapic_redirect_legacy_irq(0);
    register_irq(timer_irq,timer_stub);
    
}

void remap_PIC(uint32_t offset1, uint32_t offset2){
    outb(PIC1_COMMAND, ICW4_INIT | ICW1_ICW4); // starts the remapping sequence in cascade mode
    outb(PIC2_COMMAND, ICW4_INIT | ICW1_ICW4);

    outb(PIC1_DATA,offset1); // set new offsets
    outb(PIC2_DATA,offset2);
   
    outb(PIC1_DATA, 0x4); //slavery
    outb(PIC2_DATA, 0x2);

    outb(PIC1_DATA,ICW4_8086); //use 8086 mode
    outb(PIC2_DATA,ICW4_8086);

    //mask everything since we are using APIC now
    outb(PIC1_DATA,0xff); 
    outb(PIC2_DATA,0xff);

}

void acknowledge_PIC(uint64_t interrupt){
    if (interrupt < PIC1_START_INTERRUPT || interrupt > PIC2_START_INTERRUPT + 7){
        return;
    }

    if (interrupt >= PIC2_START_INTERRUPT)
        outb(PIC2_COMMAND,PIC_EOI);

    outb(PIC1_COMMAND,PIC_EOI);
}