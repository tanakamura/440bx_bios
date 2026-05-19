#include "acpi_tables.h"
#include "bios_memory.h"

struct acpi_table_ref {
    unsigned int sig;
    unsigned int addr;
    unsigned int len;
};

static struct acpi_table_ref acpi_refs[32];

static void acpi_memset(void* dst, unsigned char value, unsigned int len) {
    unsigned char* p = (unsigned char*)dst;
    while (len-- != 0u) {
        *p++ = value;
    }
}

__attribute__((noinline)) static void acpi_write16_linear(unsigned int addr,
                                                          unsigned short value) {
    __asm__ volatile("movw %w0, (%1)" : : "r"(value), "r"(addr) : "memory");
}

static unsigned int acpi_sig(const char* s) {
    return (unsigned int)(unsigned char)s[0] |
           ((unsigned int)(unsigned char)s[1] << 8) |
           ((unsigned int)(unsigned char)s[2] << 16) |
           ((unsigned int)(unsigned char)s[3] << 24);
}

static unsigned int acpi_get32(const unsigned char* p) {
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8) |
           ((unsigned int)p[2] << 16) | ((unsigned int)p[3] << 24);
}

static void acpi_put16(unsigned char* p, unsigned short value) {
    p[0] = (unsigned char)value;
    p[1] = (unsigned char)(value >> 8);
}

static void acpi_put32(unsigned char* p, unsigned int value) {
    p[0] = (unsigned char)value;
    p[1] = (unsigned char)(value >> 8);
    p[2] = (unsigned char)(value >> 16);
    p[3] = (unsigned char)(value >> 24);
}

static void acpi_put64(unsigned char* p, unsigned int value) {
    acpi_put32(p, value);
    acpi_put32(p + 4, 0u);
}

static void acpi_write_bytes(unsigned char* dst, const char* src,
                             unsigned int len) {
    unsigned int i;
    for (i = 0; i < len; ++i) {
        dst[i] = (unsigned char)src[i];
    }
}

static unsigned char acpi_checksum(const unsigned char* p, unsigned int len) {
    unsigned int sum = 0u;
    unsigned int i;
    for (i = 0; i < len; ++i) {
        sum += p[i];
    }
    return (unsigned char)(0u - sum);
}

static void acpi_fix_table_checksum(unsigned char* table) {
    unsigned int len = acpi_get32(table + 4);
    table[9] = 0u;
    table[9] = acpi_checksum(table, len);
}

static void acpi_install_rsdp(const char* oem_id, unsigned int rsdt_addr) {
    unsigned char* rsdp = (unsigned char*)ACPI_RSDP_LINEAR;

    acpi_memset(rsdp, 0u, 20u);
    acpi_write_bytes(rsdp + 0, "RSD PTR ", 8u);
    acpi_write_bytes(rsdp + 9, oem_id, 6u);
    rsdp[15] = 0u;
    acpi_put32(rsdp + 16, rsdt_addr);
    rsdp[8] = acpi_checksum(rsdp, 20u);
    acpi_write16_linear(0x0000040eu, ACPI_EBDA_SEGMENT);
}

static void acpi_build_header(unsigned char* table, const char* sig,
                              unsigned int len, unsigned char rev,
                              const char* oem_id, const char* table_id) {
    acpi_memset(table, 0u, len);
    acpi_write_bytes(table + 0, sig, 4u);
    acpi_put32(table + 4, len);
    table[8] = rev;
    acpi_write_bytes(table + 10, oem_id, 6u);
    acpi_write_bytes(table + 16, table_id, 8u);
    acpi_put32(table + 24, 0x00000001u);
    acpi_write_bytes(table + 28, "440B", 4u);
    acpi_put32(table + 32, 0x00000001u);
}

static void acpi_build_real_fadt(unsigned char* fadt, unsigned int facs,
                                 unsigned int dsdt) {
    acpi_build_header(fadt, "FACP", 0x74u, 1u, "ASUS  ", "P2B98-XV");
    acpi_put32(fadt + 24, 0x58582e31u);
    acpi_write_bytes(fadt + 28, "ASUS", 4u);
    acpi_put32(fadt + 32, 0x31303030u);
    acpi_put32(fadt + 36, facs);
    acpi_put32(fadt + 40, dsdt);
    acpi_put16(fadt + 46, 9u);
    acpi_put32(fadt + 48, 0u);
    fadt[52] = 0u;
    fadt[53] = 0u;
    acpi_put32(fadt + 56, ACPI_REAL_PM1_EVT);
    acpi_put32(fadt + 64, ACPI_REAL_PM1_CNT);
    acpi_put32(fadt + 76, 0x0000e408u);
    acpi_put32(fadt + 80, ACPI_REAL_GPE0);
    fadt[88] = 4u;
    fadt[89] = 2u;
    fadt[91] = 4u;
    fadt[92] = 4u;
    acpi_put16(fadt + 96, 0x005au);
    acpi_put16(fadt + 98, 0x0384u);
    fadt[104] = 1u;
    fadt[106] = 0x0du;
    acpi_put32(fadt + 112, 0x000000a5u);
    acpi_fix_table_checksum(fadt);
}

static void acpi_patch_qemu_fadt_io(unsigned char* fadt) {
    unsigned int pm1_evt = acpi_get32(fadt + 56);
    unsigned int pm1_ctl = acpi_get32(fadt + 64);
    unsigned int pm_tmr = acpi_get32(fadt + 76);
    unsigned int gpe0 = acpi_get32(fadt + 80);

    if (pm1_evt < 0x100u) {
        acpi_put32(fadt + 56, QEMU_ACPI_PM_BASE + pm1_evt);
    }
    if (pm1_ctl < 0x100u) {
        acpi_put32(fadt + 64, QEMU_ACPI_PM_BASE + pm1_ctl);
    }
    if (pm_tmr < 0x100u) {
        acpi_put32(fadt + 76, QEMU_ACPI_PM_BASE + pm_tmr);
    }
    if (gpe0 != 0u && gpe0 < 0x100u) {
        acpi_put32(fadt + 80, QEMU_ACPI_PM_BASE + gpe0);
    }
}

static int acpi_should_rsdt_ref(unsigned int sig) {
    if (sig == acpi_sig("RSDT") || sig == acpi_sig("XSDT") ||
        sig == acpi_sig("FACS") || sig == acpi_sig("DSDT")) {
        return 0;
    }
    return 1;
}

static unsigned int acpi_top_reserved_base(unsigned int total_bytes) {
    if (total_bytes <= 0x00100000u) {
        return total_bytes;
    }
    if (total_bytes <= 0x00200000u) {
        return 0x00100000u;
    }
    return (total_bytes - BIOS_TOP_RESERVED_SIZE) & ~0xfffu;
}

unsigned int acpi_table_base_for_total(unsigned int total_bytes) {
    unsigned int top = acpi_top_reserved_base(total_bytes);
    if (top >= 0x00100000u && total_bytes >= top + BIOS_TOP_RESERVED_SIZE) {
        return top + ACPI_TABLE_RESERVED_OFFSET;
    }
    return ACPI_LOW_TABLE_LINEAR;
}

unsigned int acpi_table_capacity_for_total(unsigned int total_bytes) {
    unsigned int top = acpi_top_reserved_base(total_bytes);
    if (top >= 0x00100000u && total_bytes >= top + BIOS_TOP_RESERVED_SIZE) {
        return BIOS_TOP_RESERVED_SIZE - ACPI_TABLE_RESERVED_OFFSET;
    }
    return ACPI_LOW_TABLE_CAPACITY;
}

void acpi_build_real_tables(unsigned int base, unsigned int dsdt,
                            unsigned int dsdt_size) {
    unsigned char* rsdt = (unsigned char*)base;
    unsigned char* fadt = (unsigned char*)(base + 0x0100u);
    unsigned char* facs = (unsigned char*)(base + 0x0200u);
    unsigned int fadt_addr = base + 0x0100u;
    unsigned int facs_addr = base + 0x0200u;

    (void)dsdt_size;
    acpi_build_header(rsdt, "RSDT", 40u, 1u, "ASUS  ", "P2B98-XV");
    acpi_put32(rsdt + 36, fadt_addr);
    acpi_fix_table_checksum(rsdt);

    acpi_memset(facs, 0u, 64u);
    acpi_write_bytes(facs, "FACS", 4u);
    acpi_put32(facs + 4, 64u);

    acpi_build_real_fadt(fadt, facs_addr, dsdt);
    acpi_install_rsdp("ASUS  ", base);
}

int acpi_patch_qemu_tables(unsigned int base, unsigned int size) {
    unsigned int ref_count = 0u;
    unsigned int off = 0u;
    unsigned char* rsdt = 0;
    unsigned char* fadt = 0;
    unsigned int rsdt_len = 0u;
    unsigned int fadt_len = 0u;
    unsigned int dsdt_addr = 0u;
    unsigned int facs_addr = 0u;
    unsigned int i;

    while (off + 36u <= size) {
        unsigned char* table = (unsigned char*)(base + off);
        unsigned int sig = acpi_get32(table);
        unsigned int len = acpi_get32(table + 4);
        if (len < 36u || len > size - off) {
            break;
        }
        if (sig == acpi_sig("RSDT")) {
            rsdt = table;
            rsdt_len = len;
        } else if (sig == acpi_sig("FACP")) {
            fadt = table;
            fadt_len = len;
        } else if (sig == acpi_sig("DSDT")) {
            dsdt_addr = base + off;
        } else if (sig == acpi_sig("FACS")) {
            facs_addr = base + off;
        }

        if (acpi_should_rsdt_ref(sig) &&
            ref_count < sizeof(acpi_refs) / sizeof(acpi_refs[0])) {
            acpi_refs[ref_count].sig = sig;
            acpi_refs[ref_count].addr = base + off;
            acpi_refs[ref_count].len = len;
            ++ref_count;
        }
        off += len;
    }

    if (rsdt == 0 || fadt == 0 || dsdt_addr == 0u) {
        return -1;
    }

    acpi_patch_qemu_fadt_io(fadt);
    if (fadt_len >= 44u) {
        acpi_put32(fadt + 36, facs_addr);
        acpi_put32(fadt + 40, dsdt_addr);
    }
    if (fadt_len >= 0x94u) {
        acpi_put64(fadt + 0x84u, facs_addr);
        acpi_put64(fadt + 0x8cu, dsdt_addr);
    }
    acpi_fix_table_checksum(fadt);

    if (ref_count > (rsdt_len - 36u) / 4u) {
        ref_count = (rsdt_len - 36u) / 4u;
    }
    acpi_put32(rsdt + 4, 36u + ref_count * 4u);
    for (i = 0; i < ref_count; ++i) {
        acpi_put32(rsdt + 36u + i * 4u, acpi_refs[i].addr);
    }
    acpi_fix_table_checksum(rsdt);

    off = 0u;
    while (off + 36u <= size) {
        unsigned char* table = (unsigned char*)(base + off);
        unsigned int sig = acpi_get32(table);
        unsigned int len = acpi_get32(table + 4);
        if (len < 36u || len > size - off) {
            break;
        }
        if (sig != acpi_sig("FACS")) {
            acpi_fix_table_checksum(table);
        }
        off += len;
    }

    acpi_install_rsdp("QEMU  ",
                      base + (unsigned int)(rsdt - (unsigned char*)base));
    return 0;
}
