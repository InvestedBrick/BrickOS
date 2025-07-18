
#ifndef INCLUDE_SCHEDULER_H
#define INCLUDE_SCHEDULER_H

#include "../tables/interrupts.h"
#include "../drivers/timer/pit.h"
#include "user_process.h"

typedef struct process_state_struct{
    unsigned int pid;
    interrupt_stack_frame_t regs;
    unsigned int* pd;
    struct process_state_struct* next;
    unsigned int kernel_stack_top;
} process_state_t;

#define TASK_SWITCH_DELAY_MS 20
#define TASK_SWITCH_TICKS ((TASK_SWITCH_DELAY_MS * DESIRED_STANDARD_FREQ) / 1000)


// route: user -> int handler (kernel) -> kernel 
#define RETURN_SMALLER_STACK 0xffffffff

/**
 * init_scheduler:
 * Initializes the scheduler
 */
void init_scheduler();

/**
 * init_scheduler:
 * NOTE: This function is called by the interrupt handler when initializing the scheduler
 * do not manually call
 */
void setup_kernel_process(interrupt_stack_frame_t* regs);

/**
 * switch_task:
 * Switches the current task
 */
void switch_task(interrupt_stack_frame_t* regs);
/**
 * add_process_state:
 * Adds a process to the process state linked list
 * @param usr_proc The user process struct
 */
void add_process_state(user_process_t* usr_proc);

/**
 * remove_process_state:
 * Removes a process from the process state linked list
 * @param proc The process state
 */
void remove_process_state(process_state_t* proc);

/**
 * get_process_state_by_page_dir:
 * Returns a process state with a given page dir
 * @param page_dir The page directory of the process
 */
process_state_t* get_process_state_by_page_dir(unsigned int* page_dir);


process_state_t* get_current_process_state();
#endif