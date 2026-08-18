#ifndef INCLUDE_KWORKER_H
#define INCLUDE_KWORKER_H
#include "../utilities/queue.h"

typedef void (*work_func_t)();

typedef struct {
    work_func_t func;
    void* args;
}kwork_t;

#define N_KWORKERS 2

/**
 * adds a function to the kernel work queue
 * @param func The function (if it has args cast to work_func_t)
 * @param arg up to one argument for the function
 */
void enqueue_kernel_work(work_func_t func, void* arg);

/**
 * init_kworkers:
 * Sets up work and worker queues
 */
void init_kworkers();

/**
 * add_kworker:
 * Creates a kernel worker thread and adds it to the worker queue
 */
void add_kworker();
#endif