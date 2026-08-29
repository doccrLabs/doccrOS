#include "fpu.h"

void fpu_enable(void)
{
    unsigned long cr0, cr4;

    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 &= ~((1UL << 2) | (1UL << 3)); //clear EM and TS
    cr0 |=  (1UL << 1); // set MP
    __asm__ volatile("mov %0, %%cr0" :: "r"(cr0));

    __asm__ volatile("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (1UL << 9);  // OSFXSR
    // FXSAVE/FXRSTOR + SSE
    cr4 |= (1UL << 10); // OSXMMEXCPT
    __asm__ volatile("mov %0, %%cr4" :: "r"(cr4));


    __asm__ volatile("fninit");
}

static unsigned char default_fpu_state[512] __attribute__((aligned(16)));
static int default_fpu_state_ready = 0;

void fpu_init_state(unsigned char *dst)
{
    if (!default_fpu_state_ready)
    {
        __asm__ volatile("fxsave (%0)" :: "r"(default_fpu_state) : "memory");
        default_fpu_state_ready = 1;
    }

    for (int i = 0; i < 512; i++) dst[i] = default_fpu_state[i];
}