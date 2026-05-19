#include "bios_stage3.h"

extern unsigned char __bss_start[];
extern unsigned char __bss_end[];

static void zero_bss(void) {
    unsigned char* p = __bss_start;
    while (p < __bss_end) {
        *p++ = 0u;
    }
}

void postcar_resume(unsigned int total_bytes, unsigned int aux_blob_linear) {
    bios_stage3_run(total_bytes, aux_blob_linear);
}

void bios32_entry_c(unsigned int total_bytes, unsigned int aux_blob_linear) {
    zero_bss();
    bios_stage3_run(total_bytes, aux_blob_linear);
}
