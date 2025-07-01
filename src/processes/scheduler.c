#include "scheduler.h"
#include "../memory/kmalloc.h"
#include "../util.h"
#include "../memory/memory.h"
#include "../io/log.h"

process_state_t* p_queue;
process_state_t* current_proc;
unsigned char locked = 0;

extern void kernel_process_init();

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
    p_queue->next = 0;
    current_proc = p_queue;
    locked = 1;
}

void add_process_state(unsigned int* page_dir){
    if (!p_queue) {
        error("Process queue not initialied");
        return;
    }
    process_state_t* proc = (process_state_t*)kmalloc(sizeof(process_state_t));
    proc->pd = page_dir;
    proc->next = 0;

    process_state_t* last = p_queue;

    while(last->next) {last = last->next;}

    last->next = proc;
}

void switch_task(interrupt_stack_frame_t* regs){
    // only switch when the scheduler was set up 
    if (!locked) return;

    log("Switching task");
    log("Switching from page dir");
    log_uint(*current_proc->pd);
    memcpy(&current_proc->regs, regs,sizeof(interrupt_stack_frame_t));
    if (current_proc->next) current_proc = current_proc->next;
    else current_proc = p_queue;

    log("to");
    log_uint(*current_proc->pd);
    mem_change_page_dir(current_proc->pd);
    memcpy(regs,&current_proc->regs,sizeof(interrupt_stack_frame_t));
}

void remove_process_state(process_state_t* proc){
    process_state_t* before_proc = p_queue;
    while(before_proc->next && before_proc->next != proc) before_proc = before_proc->next;

    before_proc->next = proc->next;
    // page dir is freed by kill_user_process
    kfree(proc);
}