#pragma once

#ifndef USER_LAND_H
#define USER_LAND_H

typedef struct {
    unsigned int process_id;
    unsigned int* page_dir;
} __attribute__((packed)) user_process_t;

#define USER_CODE_DATA_VMEMORY_START 0x00000000
#define USER_STACK_VMEMORY_START     0xbffffffb

#define MAX_PIDS 32768
/**
 * create_user_process:
 * Creates a User process
 * 
 * @return The proccess ID of the created process
 */
unsigned int create_user_process();
/**
 * init_user_process_vector:
 * Sets up the user process vector
 */
void init_user_process_vector();
/**
 * kill_user_process:
 * Neutralizes the user process
 */
void kill_user_process(unsigned int pid);

void enter_user_mode();
#endif