#ifndef INCLUDE_TIMER_CB_H
#define INCLUDE_TIMER_CB_H

#include <stdint.h>
#include "../utilities/vector.h"
#include "../processes/scheduler.h"

typedef struct timer_callback {
    void (*callback)();
    uint32_t period_ticks;
    uint32_t running_ticks;

    struct timer_callback* next;

}timer_callback_t;

extern vector_t timer_callbacks_vec;
extern thread_t* kworker_timer_callbacks;

/**
 * register_timer_callback:
 * registers a callback function to be called at a regular interval
 * @param callback The callback function 
 * @param period_ms The callback interval in milliseconds
 */
void register_timer_callback(void (*callback)(),uint32_t period_ms);

/**
 * unregister_timer_callback:
 * Unregisters a timer callback
 * @param callback The callback function to unregister
 */
void unregister_timer_callback(void (*callback)());

/**
 * handle_timer_callbacks:
 * Handles timer callbacks (only call from timer interrupt stub)
 */
void handle_timer_callbacks();

void kthread_worker_timer_callbacks();
#endif