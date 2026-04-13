#ifndef MISC_INSTRUC_WRAPPERS_H
#define MISC_INSTRUC_WRAPPERS_H

#include <stdint.h>
#define CPUID_HIGHEST_EXT_FUNC_IMPLEMENTED 0x80000000

#define CPUID_PROCESSOR_BRAND_STRING_0 0x80000002
#define CPUID_PROCESSOR_BRAND_STRING_1 0x80000003
#define CPUID_PROCESSOR_BRAND_STRING_2 0x80000004 

void _cpuid(uint32_t init_eax,uint32_t init_ecx,uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx);

#endif