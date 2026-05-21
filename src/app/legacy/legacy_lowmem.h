#ifndef LEGACY_LOWMEM_H
#define LEGACY_LOWMEM_H

static inline unsigned int legacy_lowmem_addr(unsigned int addr) {
    __asm__ volatile("" : "+r"(addr));
    return addr;
}

static inline volatile unsigned char* legacy_lowmem_u8(unsigned int addr) {
    return (volatile unsigned char*)legacy_lowmem_addr(addr);
}

static inline volatile unsigned short* legacy_lowmem_u16(unsigned int addr) {
    return (volatile unsigned short*)legacy_lowmem_addr(addr);
}

static inline volatile unsigned int* legacy_lowmem_u32(unsigned int addr) {
    return (volatile unsigned int*)legacy_lowmem_addr(addr);
}

#endif
