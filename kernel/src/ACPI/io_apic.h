#ifndef INCLUDE_IO_APIC_H
#define INCLUDE_IO_APIC_H

#define IOAPICID 0
#define IOAPICVER 1
#define IOAPICARB 2
#define IOREDTBL(n) (0x10 + n * 2)
#include <stdint.h>

/**
 * discover_ioapics:
 * Discovers and maps all present IO APICs
 */
void discover_ioapics(); 

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
#endif