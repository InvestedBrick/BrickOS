
#ifndef INCLUDE_SCHEDULER_H
#define INCLUDE_SCHEDULER_H

#include "../tables/interrupts.h"
#include "../drivers/timer/pit.h"
#include "user_process.h"

#define EXEC_STATE_INIT 0x1
#define EXEC_STATE_RUNNING 0x2
#define EXEC_STATE_SLEEPING 0x3
#define EXEC_STATE_DEAD 0x4

typedef struct process_state_struct{
    uint8_t exec_state;
    uint32_t pid;
    interrupt_stack_frame_t regs;
    uint64_t* pml4;
    struct process_state_struct* next;
    uint64_t kernel_stack_top;
} process_state_t;

typedef struct {
    process_state_t* proc;
    uint32_t wakeup_tick;
}sleeping_proc_t;

#define TASK_SWITCH_DELAY_MS 10
#define TASK_SWITCH_TICKS ((TASK_SWITCH_DELAY_MS * DESIRED_STANDARD_FREQ) / 1000)


/**
 * init_scheduler:
 * Initializes the scheduler
 */
void init_scheduler();

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
 * get_process_state_by_pid:
 * Returns a process state with a given pid
 * @param pid The pid of the process
 */
process_state_t* get_process_state_by_pid(uint32_t pid);

process_state_t* get_current_process_state();

/**
 * manage_sleeping_processes:
 * Wakes up sleeping processes if their wakeup tick has passed
 */
void manage_sleeping_processes();

/**
 * add_sleeping_process:
 * Add a process to the sleeping queue
 * @param proc The process to send to sleep
 * @param sleep_ticks The number of ticks to make the process sleep
 */
void add_sleeping_process(process_state_t* proc,uint32_t sleep_ticks);
#endif