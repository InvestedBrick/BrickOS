
#ifndef INCLUDE_ACPI_H
#define INCLUDE_ACPI_H

#include <stdint.h>


/**
 * init_acpi:
 * Initializes ACPI and the APIC system
 */
void init_acpi();

/**
 * acpi_shutdown:
 * Tries to power down the system using the ACPI
 */
void acpi_shutdown();

#endif