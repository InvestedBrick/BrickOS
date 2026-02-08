
#ifndef INCLUDE_ACPI_H
#define INCLUDE_ACPI_H

#define IA32_APIC_BASE_MSR 0x1B
#define IA32_APIC_BASE_MSR_BSP 0x100 // Processor is a BSP
#define IA32_APIC_BASE_MSR_ENABLE 0x800


#define APIC_REG_SPURIOUS_INT_VEC 0xf0
#define APIC_REG_EOI 0xb0

#define IOAPICID 0
#define IOAPICVER 1
#define IOAPICARB 2
#define IOREDTBL(n) (0x10 + n * 2)
#include <stdint.h>

typedef struct {
    void* apic_virt_base;
    uint8_t acpi_processor_id;
    uint8_t apic_id;
}apic_data_t;

/**
 * write_ioapic_register:
 * Writes a 32 bit value to an IO APIC register
 * @param apic_base The base address of the IO APIC
 * @param offset The register offset to write to
 * @param val The value to write
 */
void write_ioapic_register(uintptr_t apic_base,uint8_t offset, uint32_t val);

/**
 * read_ioapic_register:
 * Reads a 32 bit value from an IO APIC register
 * @param apic_base The base address of the IO APIC
 * @param offset The register offset to read from
 * 
 * @return The value read from the register
 */
uint32_t read_ioapic_register(uintptr_t apic_base,uint8_t offset);

/**
 * write_apic_register:
 * Writes a 32 bit value to an APIC register
 * @param offset The register offset to write to
 * @param value The value to write
 */
void write_apic_register(uint32_t offset, uint32_t value);

/**
 * read_apic_register:
 * Reads a 32 bit value from an APIC register
 * @param offset The register offset to read from
 * 
 * @return The value read from the register
 */
uint32_t read_apic_register(uint32_t offset);

/**
 * init_acpi:
 * Initializes ACPI and the APIC system
 */
void init_acpi();

/**
 * ioapic_redirect_irq:
 * Redirects an IRQ to the APIC system by finding the responsible IO APIC and setting up the relevant redirection table entry
 * @param irq The IRQ to redirect
 * @return The interrupt vector that the IRQ was redirected to, or 0 if the redirection failed
 */
uint8_t ioapic_redirect_irq(uint32_t irq);

void write_msr(uint32_t msr,uint32_t low, uint32_t high);
void read_msr(uint32_t msr,uint32_t* low, uint32_t* high);

#endif