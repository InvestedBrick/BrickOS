#pragma once

#ifndef INCLUDE_GDT_H
#define INCLUDE_GDT_H

struct gdt {
    unsigned short size;
    unsigned int address;
} __attribute__((packed));

struct gdt_entry {
    unsigned short limit_low;
    unsigned short base_low;
    unsigned char base_middle;
    unsigned char access;
    unsigned char granularity;
    unsigned char base_high;
}__attribute__((packed));

/**
 * load_gdt - loads global descriptor table
 * @param table A pointer to a gdt
 * 
 */
void load_gdt(const struct gdt* table);

void set_gdt_entry(int num,unsigned int base, unsigned int limit, unsigned char access, unsigned char gran);

void init_gdt();
#endif
