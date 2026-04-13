#include "misc_instruc_wrappers.h"

void _cpuid(uint32_t init_eax,uint32_t init_ecx,uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx)
{
    asm volatile("cpuid"
        : "=a" (*eax),
          "=b" (*ebx),
          "=c" (*ecx),
          "=d" (*edx)
        : "a" (init_eax), "c" (init_ecx));
}
