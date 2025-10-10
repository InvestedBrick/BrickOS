
#ifndef INCLUDE_TSS_H
#define INCLUDE_TSS_H

typedef struct {
    uint32_t reserved1;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved2;
    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t reserved3;
    uint16_t reserved4;
    uint16_t iobp;
} __attribute__((packed)) tss_entry_t;

void write_tss(uint64_t rsp0);
void set_kernel_stack(uint64_t stack);

#endif