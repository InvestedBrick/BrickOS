#include "timer_callbacks.h"
#include "../processes/scheduler.h"
#include "../memory/kmalloc.h"
#include "../io/log.h"
#include <stdint.h>

timer_callback_t* timer_callback_head;
vector_t timer_callbacks_vec;
thread_t* kworker_timer_callbacks;

void kthread_worker_timer_callbacks(){
    while(1){
        enable_interrupts(); // gets disabled by the iretq from kernel to kernel
        
        while (timer_callbacks_vec.size > 0){
            void (*cb)() = (void (*)())vector_pop(&timer_callbacks_vec);
            cb();
        }
        
        add_sleeping_thread(kworker_timer_callbacks, THREAD_ETERNAL_SLEEP);
        invoke_scheduler();
    }
}

void handle_timer_callbacks(){
    timer_callback_t* head = timer_callback_head;
    while(head){
        head->running_ticks++;
        if (head->running_ticks >= head->period_ticks){
            head->running_ticks = 0;
            vector_append(&timer_callbacks_vec,(vector_data_t)head->callback);
        }

        head = head->next;
    } 
    if (timer_callbacks_vec.size > 0) wakeup_thread(kworker_timer_callbacks);
}

void register_timer_callback(void (*callback)(),uint32_t period_ms){
    uint8_t status = get_interrupt_status();
    disable_interrupts();
   
    timer_callback_t* cb = (timer_callback_t*)kmalloc(sizeof(timer_callback_t));
    cb->callback = callback;
    cb->period_ticks = MS_TO_TICKS(period_ms);
    cb->running_ticks = 0;
    cb->next = nullptr;
    
    if (!timer_callback_head) timer_callback_head = cb;
    else{
        timer_callback_t* prev = timer_callback_head;
        while(prev->next) prev = prev->next;
        prev->next = cb;
    }

    set_interrupt_status(status);
}

void unregister_timer_callback(void (*callback)()){
    uint8_t status = get_interrupt_status();
    disable_interrupts();

    if (!timer_callback_head) return;

    timer_callback_t* curr = timer_callback_head;
    if (timer_callback_head->callback == callback){
        timer_callback_head = timer_callback_head->next;
        kfree(curr);
        return;
    }

    while(curr->next && curr->next->callback != callback) curr = curr->next;

    if (curr->next){
        timer_callback_t* to_del = curr->next;
        curr->next = to_del->next;
        kfree(to_del);
    }

    set_interrupt_status(status);
}