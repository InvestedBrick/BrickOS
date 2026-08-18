#include "kworker.h"
#include "../memory/kmalloc.h"
#include "scheduler.h"
#include "../io/log.h"

queue_t work_queue;
queue_t worker_queue;

void kwork(){
    thread_t* worker = get_current_thread();
    while(true){
        while(true){
            uint32_t f = irq_save();
            if (work_queue.size == 0){
                irq_restore(f);
                break;
            }
            
            kwork_t* work = (kwork_t*)queue_pop(&work_queue);
            irq_restore(f);

            if (work->args){
                void (*func)(void*) = ((void (*)(void*))work->func);
                func(work->args);
            }else {
                work->func();
            }
            kfree(work);
        }
        uint32_t f = irq_save();
        if (work_queue.size > 0){
            irq_restore(f);
            continue;
        }
        add_sleeping_thread(worker,THREAD_ETERNAL_SLEEP);
        queue_push(&worker_queue,(queue_data_t)worker);
        irq_restore(f);
        yield();
    }
}

void enqueue_kernel_work(work_func_t func, void* args){
    kwork_t* work = (kwork_t*)kmalloc(sizeof(kwork_t));
    work->func = func;
    work->args = args;

    uint32_t f = irq_save();
    queue_push(&work_queue, (queue_data_t)work);
    thread_t* t = nullptr;
    if (worker_queue.size > 0) t = (thread_t*)queue_pop(&worker_queue);
    irq_restore(f);

    if (t) wakeup_thread(t);

}

void init_kworkers(){
    init_queue(&work_queue);
    init_queue(&worker_queue);
}

void add_kworker(){
    thread_t* worker = create_kernel_worker_thread(kwork);
    uint32_t f = irq_save();
    queue_push(&worker_queue,(queue_data_t)worker);
    irq_restore(f);
}