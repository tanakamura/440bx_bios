#include "stage3.h"

extern unsigned char __bss_start[];
extern unsigned char __bss_end[];

static void zero_bss(void) {
    unsigned char* p = __bss_start;
    while (p < __bss_end) {
        *p++ = 0u;
    }
}

__attribute__((section(".entry"), used)) void
postcar_resume(unsigned int total_bytes) {
    bios_stage3_run(total_bytes);
}

void bios32_entry_c(unsigned int total_bytes, unsigned int unused) {
    (void)unused;
    zero_bss();
    bios_stage3_run(total_bytes);
}
