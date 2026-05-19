#include "acpi_tables.h"
#include "blob.h"
#include "l2_service.h"
#include "shared_service/service_table.h"

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

static inline void outw(unsigned short port, unsigned short value) {
    __asm__ volatile("outw %0, %1" : : "a"(value), "Nd"(port));
}

static inline void outl(unsigned short port, unsigned int value) {
    __asm__ volatile("outl %0, %1" : : "a"(value), "Nd"(port));
}

static inline unsigned int inl(unsigned short port) {
    unsigned int value;
    __asm__ volatile("inl %1, %0" : "=a"(value) : "Nd"(port));
    return value;
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

static void pci_write16(unsigned char bus, unsigned char dev, unsigned char fn,
                        unsigned char reg, unsigned short value) {
    outl(0x0cf8u, pci_addr(bus, dev, fn, reg));
    outw((unsigned short)(0x0cfcu + (reg & 0x02u)), value);
}

static void pci_write32(unsigned char bus, unsigned char dev, unsigned char fn,
                        unsigned char reg, unsigned int value) {
    outl(0x0cf8u, pci_addr(bus, dev, fn, reg));
    outl(0x0cfcu, value);
}

static unsigned int pci_read32(unsigned char bus, unsigned char dev,
                               unsigned char fn, unsigned char reg) {
    outl(0x0cf8u, pci_addr(bus, dev, fn, reg));
    return inl(0x0cfcu);
}

static unsigned short pci_read16(unsigned char bus, unsigned char dev,
                                 unsigned char fn, unsigned char reg) {
    unsigned int value = pci_read32(bus, dev, fn, reg);
    return (unsigned short)(value >> ((reg & 0x02u) * 8u));
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

static void clear_app_shadow_window(void) {
    volatile unsigned int* p = (volatile unsigned int*)0x000c0000u;
    volatile unsigned int* end = (volatile unsigned int*)0x000fe000u;

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

static void enable_platform_pm_io(struct shared_boot_context* boot_ctx) {
    unsigned short cmd;
    unsigned char misc;

    if (pci_read32(0, ACPI_REAL_PCI_DEV, ACPI_REAL_PCI_FN, 0x00) !=
        0x71138086u) {
        serial_write_string("ACPI PM dev missing\r\n");
        return;
    }

    pci_write32(0, ACPI_REAL_PCI_DEV, ACPI_REAL_PCI_FN, 0x40,
                ACPI_REAL_PM1_EVT | 0x00000001u);
    misc = pci_read8(0, ACPI_REAL_PCI_DEV, ACPI_REAL_PCI_FN, 0x80);
    pci_write8(0, ACPI_REAL_PCI_DEV, ACPI_REAL_PCI_FN, 0x80,
               (unsigned char)(misc | 0x81u));
    cmd = pci_read16(0, ACPI_REAL_PCI_DEV, ACPI_REAL_PCI_FN, 0x04);
    pci_write16(0, ACPI_REAL_PCI_DEV, ACPI_REAL_PCI_FN, 0x04,
                (unsigned short)(cmd | 0x0001u));

    if (boot_ctx != 0) {
        boot_ctx->acpi_pm1_evt = ACPI_REAL_PM1_EVT;
        boot_ctx->acpi_pm1_cnt = ACPI_REAL_PM1_CNT;
        boot_ctx->acpi_gpe0 = ACPI_REAL_GPE0;
        boot_ctx->acpi_gpe0_len = ACPI_GPE0_LEN;
        boot_ctx->acpi_flags = SHARED_BOOT_ACPI_FLAG_ENABLE_SCI;
    }

    serial_write_string("ACPI PM io pmb=");
    serial_write_hex32(
        pci_read32(0, ACPI_REAL_PCI_DEV, ACPI_REAL_PCI_FN, 0x40));
    serial_write_string(" misc=");
    serial_write_hex8(pci_read8(0, ACPI_REAL_PCI_DEV, ACPI_REAL_PCI_FN, 0x80));
    serial_write_string(" cmd=");
    serial_write_hex16(
        pci_read16(0, ACPI_REAL_PCI_DEV, ACPI_REAL_PCI_FN, 0x04));
    serial_write_string("\r\n");
}

static void install_real_acpi_tables(unsigned int total_bytes,
                                     struct shared_boot_context* boot_ctx,
                                     blob_expand_fn expand,
                                     unsigned int blob_stage) {
    unsigned int dsdt_blob = 0u;
    unsigned int base = acpi_table_base_for_total(total_bytes);
    unsigned int cap = acpi_table_capacity_for_total(total_bytes);
    unsigned int dsdt = base + 0x1000u;
    struct blob_status status;
    int rc;

    if (boot_ctx != 0) {
        dsdt_blob = boot_ctx->acpi_input_ptr;
    }
    if (dsdt_blob == 0u || cap <= 0x1000u || expand == 0 ||
        blob_stage == 0u) {
        serial_write_string("ACPI real tables skipped\r\n");
        return;
    }

    serial_write_string("ACPI real DSDT @ ");
    serial_write_hex32(base);
    serial_write_string("...");
    rc = expand((const void*)dsdt_blob, (void*)blob_stage, (void*)dsdt,
                cap - 0x1000u, &status, total_bytes);
    serial_write_string("\r\n");
    if (rc != 0) {
        serial_write_string("ACPI DSDT blob failed rc=");
        serial_write_hex8((unsigned char)rc);
        serial_write_string(" block=");
        serial_write_hex32(status.block);
        serial_write_string("\r\n");
        return;
    }

    acpi_build_real_tables(base, dsdt, status.output_size);
    if (boot_ctx != 0) {
        boot_ctx->rsdp_linear = ACPI_RSDP_LINEAR;
    }
    enable_platform_pm_io(boot_ctx);
    serial_write_string("ACPI real tables ok\r\n");
}

__attribute__((section(".stage2.entry"), used)) void stage2_entry(
    unsigned int total_bytes, unsigned int aux_linear) {
    typedef void (*bios_entry_fn)(unsigned int, unsigned int);
    struct shared_service_table* service =
        shared_service_from_total(total_bytes);
    struct shared_boot_context* boot_ctx = shared_boot_context(service);
    struct shared_payload_entry* stage3_payload =
        shared_payload_find(service, SHARED_PAYLOAD_ID_STAGE3);
    const void* stage3_blob = 0;
    blob_expand_fn expand = 0;
    unsigned int blob_stage = 0u;
    struct blob_status status;
    int rc;

    if (service != 0 && service->blob_expand != 0u) {
        expand = (blob_expand_fn)service->blob_expand;
        blob_stage = service->blob_stage;
    }
    if (stage3_payload != 0) {
        stage3_blob = (const void*)stage3_payload->blob_ptr;
    }

    zero_stage2_bss();
    serial_write_string("stage2 @ 00080000\r\n");
    init_l2_cache();

    serial_write_string("Stage2 PAM/MTRR...\r\n");
    enable_shadow_ram_and_wb();
    clear_app_shadow_window();
    clear_stage3_window();
    if (boot_ctx != 0) {
        boot_ctx->flags |= SHARED_BOOT_FLAG_SHADOW_READY;
    }
    install_real_acpi_tables(total_bytes, boot_ctx, expand, blob_stage);

    serial_write_string("Load stage3 @ 00200000...\r\n");
    if (stage3_blob == 0 || expand == 0 || blob_stage == 0u ||
        service->blob_stage_size < BLOB_STAGE_CAPACITY) {
        serial_write_string("stage3 blob missing\r\n");
        outb(0x80u, 0xefu);
        for (;;) {
            __asm__ volatile("hlt");
        }
    }
    rc = expand(stage3_blob, (void*)blob_stage, (void*)BIOS_LOAD_LINEAR,
                BIOS_LOAD_CAPACITY, &status, total_bytes);
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
