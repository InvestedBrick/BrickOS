#include "scheduler.h"
#include "../memory/kmalloc.h"
#include "../util.h"
#include "../memory/memory.h"
#include "../io/log.h"
#include "user_process.h"
#include "../tables/tss.h"
#include "../kernel_process.h"
process_state_t* p_queue;
process_state_t* current_proc;
unsigned char locked = 0;
extern unsigned int stack_top;
extern void kernel_process_init();

process_state_t* get_current_process_state(){
    return current_proc;
}

void init_scheduler(){
    p_queue = 0;
    current_proc = 0;
    
    kernel_process_init();
}

process_state_t* get_process_state_by_page_dir(unsigned int* page_dir){
    process_state_t* node = p_queue;
    while (node) {
        if (node->pd == page_dir) return node;
        node = node->next;
    }
    return 0;
}

// This function is called by the interrupt handler, do not manually call
void setup_kernel_process(interrupt_stack_frame_t* regs){
    if (locked){
        warn("Setup kernel process called again!");
        return;
    }
    p_queue = (process_state_t*)kmalloc(sizeof(process_state_t));
    p_queue->pd = mem_get_current_page_dir();
    memcpy(&p_queue->regs,regs,sizeof(interrupt_stack_frame_t));
    p_queue->pid = global_kernel_process.process_id;
    p_queue->kernel_stack_top = stack_top;
    p_queue->next = 0;
    current_proc = p_queue;
    locked = 1;
    log("Set up kernel process");
}

void add_process_state(user_process_t* usr_proc){
    if (!p_queue) {
        error("Process queue not initialized");
        return;
    }
    process_state_t* proc = (process_state_t*)kmalloc(sizeof(process_state_t));
    proc->pd = usr_proc->page_dir;
    proc->next = 0;
    proc->kernel_stack_top = usr_proc->kernel_stack + MEMORY_PAGE_SIZE;
    proc->pid = usr_proc->process_id;
    process_state_t* last = p_queue;

    while(last->next) {last = last->next;}

    last->next = proc;
}
int breakpoint(int num){
    return num + 1;
}
void switch_task(interrupt_stack_frame_t* regs){
    // only switch when the scheduler was set up 
    if (!locked) return;
    unsigned int* old_pd = mem_get_current_page_dir();

    unsigned int interrupt_code = regs->interrupt_number;
    memcpy(&current_proc->regs, regs,sizeof(interrupt_stack_frame_t));

    if (current_proc->next) current_proc = current_proc->next;
    else current_proc = p_queue;

    if (current_proc->pd != old_pd){
        int x = breakpoint(3);
    }

    if (current_proc != p_queue){
        set_kernel_stack(current_proc->kernel_stack_top);
    }

    mem_change_page_dir(current_proc->pd);
    memcpy(regs,&current_proc->regs,sizeof(interrupt_stack_frame_t));
    regs->interrupt_number = interrupt_code; 
}

void remove_process_state(process_state_t* proc){
    process_state_t* before_proc = p_queue;
    while(before_proc->next && before_proc->next != proc) before_proc = before_proc->next;

    before_proc->next = proc->next;
    // page dir is freed by kill_user_process
    kfree(proc);
}