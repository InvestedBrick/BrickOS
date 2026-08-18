#include "timer_callbacks.h"
#include "../processes/scheduler.h"
#include "../processes/kworker.h"
#include "../memory/kmalloc.h"
#include "../io/log.h"
#include <stdint.h>

timer_callback_t* timer_callback_head;

void handle_timer_callbacks(){
    timer_callback_t* head = timer_callback_head;
    while(head){
        head->running_ticks++;
        if (head->running_ticks >= head->period_ticks){
            head->running_ticks = 0;
            enqueue_kernel_work(head->callback,nullptr);
        }

        head = head->next;
    } 
}

void register_timer_callback(void (*callback)(),uint32_t period_ms){
    uint32_t f = irq_save();
   
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

    irq_restore(f);
}

void unregister_timer_callback(void (*callback)()){
    uint32_t f = irq_save();

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

    irq_restore(f);
}