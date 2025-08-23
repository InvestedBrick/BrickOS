
#ifndef INCLUDE_KERNEL_SHELL
#define INCLUDE_KERNEL_SHELL

#include "../util.h"

typedef struct {
    string_t command;
    uint32_t n_args;
    string_t* args;
}command_t;
/**
 * start_shell:
 * Starts a really simple shell
 */
void start_shell();

#endif