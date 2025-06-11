#pragma once

#ifndef INCLUDE_TSS_H
#define INCLUDE_TSS_H

typedef struct {
    unsigned int prev_tss;
    unsigned int esp0;
    unsigned int ss0;
    unsigned int esp1;
    unsigned int ss1;
    unsigned int esp2;
    unsigned int ss2;
    unsigned int cr3;
    unsigned int eip;
    unsigned int eflags;
    unsigned int eax, ecx, edx, ebx;
    unsigned int esp, ebp, esi, edi;
    unsigned int es, cs, ss, ds, fs, gs;
    unsigned int ldt;
    unsigned short trap;
    unsigned short iomap_base;
} __attribute__((packed)) tss_entry_t;

void write_tss(unsigned short ss0, unsigned int esp0);
void set_kernel_stack(unsigned int stack);

#endif