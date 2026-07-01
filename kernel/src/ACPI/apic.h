#ifndef INCLUDE_APIC_H
#define INCLUDE_APIC_H

#include <stdint.h>

#define IA32_APIC_BASE_MSR 0x1B
#define IA32_APIC_BASE_MSR_BSP 0x100 // Processor is a BSP
#define IA32_APIC_BASE_MSR_ENABLE 0x800


#define APIC_REG_SPURIOUS_INT_VEC 0xf0
#define APIC_REG_EOI 0xb0

#define IOAPICID 0
#define IOAPICVER 1
#define IOAPICARB 2
#define IOREDTBL(n) (0x10 + n * 2)

typedef struct {
    void* apic_virt_base;
    uint8_t acpi_processor_id;
    uint8_t apic_id;
}apic_data_t;

/**
 * initialize_apics:
 * Discoveres the IO APICs and sets up the LAPIC
 */
void initialize_apics();

void write_msr(uint32_t msr,uint32_t low, uint32_t high);
void read_msr(uint32_t msr,uint32_t* low, uint32_t* high);
#endif