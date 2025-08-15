#ifndef INCLUDE_SHELL_H
#define INCLUDE_SHELL_H
#include "cstdlib/stdutils.h"
typedef struct {
    char* executing_dir;
    string_t command;
    unsigned int n_args;
    string_t* args;
}command_t;

typedef void (*command_handler_t)(command_t*);

typedef struct {
    const char* name;
    command_handler_t handler;
    const char* help;
}shell_command_t;

void cmd_help(command_t*);
void cmd_clear(command_t*);
void cmd_ls(command_t*);
void cmd_cd(command_t*);
void cmd_mkf(command_t*);
void cmd_mkdir(command_t*);
void cmd_rm(command_t*);
#endif