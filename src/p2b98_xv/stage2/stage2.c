#include "blob.h"
#include "l2_service.h"

extern unsigned char __stage2_bss_start[];
extern unsigned char __stage2_bss_end[];

static inline void outb(unsigned short port, unsigned char value) {
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline unsigned char inb(unsigned short port) {
    unsigned char value;
    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline void outl(unsigned short port, unsigned int value) {
    __asm__ volatile("outl %0, %1" : : "a"(value), "Nd"(port));
}

static void serial_write_char(char c) {
    while ((inb(0x03f8u + 5u) & 0x20u) == 0) {
    }
    outb(0x03f8u, (unsigned char)c);
}

static void serial_write_string(const char* s) {
    while (*s != '\0') {
        serial_write_char(*s);
        ++s;
    }
}

static void serial_write_hex4(unsigned char value) {
    value &= 0x0f;
    serial_write_char(
        (char)(value < 10 ? ('0' + value) : ('a' + (value - 10))));
}

static void serial_write_hex8(unsigned char value) {
    serial_write_hex4((unsigned char)(value >> 4));
    serial_write_hex4(value);
}

static void serial_write_hex16(unsigned short value) {
    serial_write_hex8((unsigned char)(value >> 8));
    serial_write_hex8((unsigned char)value);
}

static void serial_write_hex32(unsigned int value) {
    serial_write_hex16((unsigned short)(value >> 16));
    serial_write_hex16((unsigned short)value);
}

static void serial_write_u32(unsigned int value) {
    char buf[10];
    unsigned int i = 0;

    if (value == 0) {
        serial_write_char('0');
        return;
    }

    while (value != 0 && i < sizeof(buf)) {
        buf[i++] = (char)('0' + (value % 10u));
        value /= 10u;
    }
    while (i != 0) {
        serial_write_char(buf[--i]);
    }
}

static unsigned int pci_addr(unsigned char bus, unsigned char dev,
                             unsigned char fn, unsigned char reg) {
    return 0x80000000u | ((unsigned int)bus << 16) |
           ((unsigned int)dev << 11) | ((unsigned int)fn << 8) |
           (reg & 0xfcu);
}

static void pci_write8(unsigned char bus, unsigned char dev, unsigned char fn,
                       unsigned char reg, unsigned char value) {
    outl(0x0cf8u, pci_addr(bus, dev, fn, reg));
    outb((unsigned short)(0x0cfcu + (reg & 3u)), value);
}

static unsigned char pci_read8(unsigned char bus, unsigned char dev,
                               unsigned char fn, unsigned char reg) {
    outl(0x0cf8u, pci_addr(bus, dev, fn, reg));
    return inb((unsigned short)(0x0cfcu + (reg & 3u)));
}

static unsigned long long rdmsr64(unsigned int msr) {
    unsigned int lo;
    unsigned int hi;
    __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((unsigned long long)hi << 32) | lo;
}

static void wrmsr64(unsigned int msr, unsigned int lo, unsigned int hi) {
    __asm__ volatile("wrmsr" : : "c"(msr), "a"(lo), "d"(hi));
}

#define IA32_MTRR_FIX4K_C0000 0x268u
#define IA32_MTRR_FIX4K_C8000 0x269u
#define IA32_MTRR_FIX4K_D0000 0x26au
#define IA32_MTRR_FIX4K_D8000 0x26bu
#define IA32_MTRR_FIX4K_E0000 0x26cu
#define IA32_MTRR_FIX4K_E8000 0x26du
#define IA32_MTRR_FIX4K_F0000 0x26eu
#define IA32_MTRR_FIX4K_F8000 0x26fu
#define IA32_MTRR_DEF_TYPE 0x2ffu
#define MTRR_DEF_TYPE_E 0x00000800u

static void enable_shadow_ram_and_wb(void) {
    unsigned long long def_type = rdmsr64(IA32_MTRR_DEF_TYPE);
    unsigned int def_lo = (unsigned int)def_type;
    unsigned int def_hi = (unsigned int)(def_type >> 32);
    unsigned char old_pam0;
    unsigned char pam;

    __asm__ volatile("movl %%cr0, %%eax\n\t"
                     "orl $0x40000000, %%eax\n\t"
                     "andl $0xdfffffff, %%eax\n\t"
                     "movl %%eax, %%cr0\n\t"
                     "wbinvd"
                     :
                     :
                     : "eax", "memory");

    old_pam0 = pci_read8(0, 0, 0, 0x59u);
    pci_write8(0, 0, 0, 0x59u, (unsigned char)(old_pam0 | 0x30u));
    for (pam = 0x5au; pam <= 0x5fu; ++pam) {
        pci_write8(0, 0, 0, pam, 0x33u);
    }

    wrmsr64(IA32_MTRR_DEF_TYPE, def_lo & ~MTRR_DEF_TYPE_E, def_hi);
    wrmsr64(IA32_MTRR_FIX4K_C0000, 0x06060606u, 0x06060606u);
    wrmsr64(IA32_MTRR_FIX4K_C8000, 0x06060606u, 0x06060606u);
    wrmsr64(IA32_MTRR_FIX4K_D0000, 0x06060606u, 0x06060606u);
    wrmsr64(IA32_MTRR_FIX4K_D8000, 0x06060606u, 0x06060606u);
    wrmsr64(IA32_MTRR_FIX4K_E0000, 0x06060606u, 0x06060606u);
    wrmsr64(IA32_MTRR_FIX4K_E8000, 0x06060606u, 0x06060606u);
    wrmsr64(IA32_MTRR_FIX4K_F0000, 0x06060606u, 0x06060606u);
    wrmsr64(IA32_MTRR_FIX4K_F8000, 0x06060606u, 0x06060606u);
    wrmsr64(IA32_MTRR_DEF_TYPE, def_lo, def_hi);

    __asm__ volatile("wbinvd\n\t"
                     "movl %%cr0, %%eax\n\t"
                     "andl $0x9fffffff, %%eax\n\t"
                     "movl %%eax, %%cr0\n\t"
                     "xorl %%eax, %%eax\n\t"
                     "cpuid"
                     :
                     :
                     : "eax", "ebx", "ecx", "edx", "memory");

    serial_write_string("PAM shadow RAM C-F old=");
    serial_write_hex8(old_pam0);
    serial_write_string(" new=");
    serial_write_hex8(pci_read8(0, 0, 0, 0x59u));
    serial_write_string("\r\n");
    serial_write_string("MTRR shadow C-F WB\r\n");
}

static void clear_option_rom_shadow_window(void) {
    volatile unsigned int* p = (volatile unsigned int*)0x000c0000u;
    volatile unsigned int* end = (volatile unsigned int*)0x000f0000u;

    while (p < end) {
        *p++ = 0u;
    }
}

static void clear_stage3_window(void) {
    volatile unsigned int* p = (volatile unsigned int*)BIOS_LOAD_LINEAR;
    volatile unsigned int* end =
        (volatile unsigned int*)(BIOS_LOAD_LINEAR + BIOS_LOAD_CAPACITY);

    while (p < end) {
        *p++ = 0u;
    }
}

static void zero_stage2_bss(void) {
    unsigned char* p = __stage2_bss_start;

    while (p < __stage2_bss_end) {
        *p++ = 0u;
    }
}

static void init_l2_cache(void) {
    struct l2_status status;
    int rc;

    serial_write_string("Stage2 L2 init...\r\n");
    rc = l2_enable_service(&status);
    serial_write_string("L2 sizefield=");
    serial_write_u32(status.total_kb);
    serial_write_string("K\r\n");
    serial_write_string("L2 alias@size=");
    serial_write_hex16((unsigned short)status.alias_rc);
    serial_write_string("\r\n");
    serial_write_string("L2 rc=");
    serial_write_hex8((unsigned char)rc);
    serial_write_string(" BBL_CR_CTL3*=");
    serial_write_hex32(status.after_ctl3);
    serial_write_string("\r\n");
}

__attribute__((section(".stage2.entry"), used)) void stage2_entry(
    unsigned int total_bytes, unsigned int aux_linear) {
    typedef void (*bios_entry_fn)(unsigned int, unsigned int);
    volatile unsigned int* aux = (volatile unsigned int*)aux_linear;
    const void* stage3_blob = (const void*)aux[BOOT_AUX_STAGE3_BLOB];
    blob_expand_fn expand = (blob_expand_fn)BLOB_SERVICE_LINEAR;
    struct blob_status status;
    int rc;

    zero_stage2_bss();
    serial_write_string("stage2 @ 00080000\r\n");
    init_l2_cache();

    serial_write_string("Stage2 PAM/MTRR...\r\n");
    enable_shadow_ram_and_wb();
    clear_option_rom_shadow_window();
    clear_stage3_window();
    aux[BOOT_AUX_SHADOW_READY] = 1u;

    serial_write_string("Load stage3 @ 000f0000...\r\n");
    rc = expand(stage3_blob, (void*)BLOB_STAGE_LINEAR, (void*)BIOS_LOAD_LINEAR,
                BIOS_LOAD_CAPACITY, &status);
    if (rc != 0) {
        serial_write_string("\r\nstage3 load failed rc=");
        serial_write_hex8((unsigned char)rc);
        serial_write_string(" block=");
        serial_write_hex32(status.block);
        serial_write_string(" exp=");
        serial_write_hex32(status.expected);
        serial_write_string(" got=");
        serial_write_hex32(status.got);
        serial_write_string("\r\n");
        outb(0x80u, 0xefu);
        for (;;) {
            __asm__ volatile("hlt");
        }
    }

    serial_write_string("\r\nstage3 copied\r\n");
    ((bios_entry_fn)BIOS32_ENTRY)(total_bytes, aux_linear);
    for (;;) {
        __asm__ volatile("hlt");
    }
}
