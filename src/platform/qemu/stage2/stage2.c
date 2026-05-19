#include "acpi_tables.h"
#include "blob.h"
#include "shared_service/service_table.h"

extern unsigned char __qemu_stage2_bss_start[];
extern unsigned char __qemu_stage2_bss_end[];

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
    value &= 0x0fu;
    serial_write_char(
        (char)(value < 10 ? ('0' + value) : ('a' + (value - 10))));
}

static void serial_write_hex8(unsigned char value) {
    serial_write_hex4((unsigned char)(value >> 4));
    serial_write_hex4(value);
}

static void serial_write_hex32(unsigned int value) {
    serial_write_hex8((unsigned char)(value >> 24));
    serial_write_hex8((unsigned char)(value >> 16));
    serial_write_hex8((unsigned char)(value >> 8));
    serial_write_hex8((unsigned char)value);
}

static void zero_bss(void) {
    unsigned char* p = __qemu_stage2_bss_start;
    while (p < __qemu_stage2_bss_end) {
        *p++ = 0u;
    }
}

static unsigned int pci_addr(unsigned char bus, unsigned char dev,
                             unsigned char fn, unsigned char reg) {
    return 0x80000000u | ((unsigned int)bus << 16) |
           ((unsigned int)dev << 11) | ((unsigned int)fn << 8) |
           (reg & 0xfcu);
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

static void pci_write32(unsigned char bus, unsigned char dev, unsigned char fn,
                        unsigned char reg, unsigned int value) {
    outl(0x0cf8u, pci_addr(bus, dev, fn, reg));
    outl(0x0cfcu, value);
}

static void pci_write16(unsigned char bus, unsigned char dev, unsigned char fn,
                        unsigned char reg, unsigned short value) {
    outl(0x0cf8u, pci_addr(bus, dev, fn, reg));
    outw((unsigned short)(0x0cfcu + (reg & 0x02u)), value);
}

static void pci_write8(unsigned char bus, unsigned char dev, unsigned char fn,
                       unsigned char reg, unsigned char value) {
    outl(0x0cf8u, pci_addr(bus, dev, fn, reg));
    outb((unsigned short)(0x0cfcu + (reg & 0x03u)), value);
}

#define FW_CFG_PORT_SEL 0x0510u
#define FW_CFG_PORT_DATA 0x0511u
#define FW_CFG_SIGNATURE 0x0000u
#define FW_CFG_FILE_DIR 0x0019u
#define FW_CFG_MAX_FILE_PATH 56u

static int stage2_name_eq(const char* a, const char* b) {
    while (*a != '\0' || *b != '\0') {
        if (*a != *b) {
            return 0;
        }
        ++a;
        ++b;
    }
    return 1;
}

static void fwcfg_select(unsigned short selector) {
    outw(FW_CFG_PORT_SEL, selector);
}

static unsigned char fwcfg_read8(void) { return inb(FW_CFG_PORT_DATA); }

static unsigned short fwcfg_read_be16(void) {
    unsigned short hi = fwcfg_read8();
    unsigned short lo = fwcfg_read8();
    return (unsigned short)((hi << 8) | lo);
}

static unsigned int fwcfg_read_be32(void) {
    unsigned int b0 = fwcfg_read8();
    unsigned int b1 = fwcfg_read8();
    unsigned int b2 = fwcfg_read8();
    unsigned int b3 = fwcfg_read8();
    return (b0 << 24) | (b1 << 16) | (b2 << 8) | b3;
}

static int fwcfg_signature_ok(void) {
    fwcfg_select(FW_CFG_SIGNATURE);
    return fwcfg_read8() == 'Q' && fwcfg_read8() == 'E' &&
           fwcfg_read8() == 'M' && fwcfg_read8() == 'U';
}

static int fwcfg_find_file(const char* name, unsigned short* selector,
                           unsigned int* size) {
    unsigned int count;
    unsigned int i;

    if (!fwcfg_signature_ok()) {
        return -1;
    }
    fwcfg_select(FW_CFG_FILE_DIR);
    count = fwcfg_read_be32();
    for (i = 0; i < count; ++i) {
        unsigned int file_size = fwcfg_read_be32();
        unsigned short file_select = fwcfg_read_be16();
        char file_name[FW_CFG_MAX_FILE_PATH];
        unsigned int j;

        (void)fwcfg_read_be16();
        for (j = 0; j < FW_CFG_MAX_FILE_PATH; ++j) {
            file_name[j] = (char)fwcfg_read8();
        }
        file_name[FW_CFG_MAX_FILE_PATH - 1u] = '\0';
        if (stage2_name_eq(file_name, name)) {
            *selector = file_select;
            *size = file_size;
            return 0;
        }
    }
    return -1;
}

static void fwcfg_read_file(unsigned short selector, void* dst,
                            unsigned int size) {
    unsigned char* p = (unsigned char*)dst;
    unsigned int i;

    fwcfg_select(selector);
    for (i = 0; i < size; ++i) {
        p[i] = fwcfg_read8();
    }
}

static void acpi_enable_qemu_pm_io(void) {
    if (pci_read32(0, 1, 3, 0x00) != 0x71138086u) {
        return;
    }
    pci_write32(0, 1, 3, 0x40, QEMU_ACPI_PM_BASE | 0x00000001u);
    pci_write8(0, 1, 3, 0x80, 0x81u);
    pci_write16(0, 1, 3, 0x04,
                (unsigned short)(pci_read16(0, 1, 3, 0x04) | 0x0001u));
}

static void install_qemu_acpi_tables(unsigned int total_bytes,
                                     struct shared_boot_context* boot_ctx) {
    unsigned short selector;
    unsigned int size;
    unsigned int base = acpi_table_base_for_total(total_bytes);
    unsigned int cap = acpi_table_capacity_for_total(total_bytes);

    if (fwcfg_find_file("etc/acpi/tables", &selector, &size) != 0 ||
        size == 0u || size > cap) {
        serial_write_string("ACPI qemu tables skipped\r\n");
        return;
    }

    serial_write_string("ACPI qemu fw_cfg @ ");
    serial_write_hex32(base);
    serial_write_string(" size=");
    serial_write_hex32(size);
    serial_write_string("\r\n");
    acpi_enable_qemu_pm_io();
    fwcfg_read_file(selector, (void*)base, size);
    if (acpi_patch_qemu_tables(base, size) != 0) {
        serial_write_string("ACPI qemu patch failed\r\n");
        return;
    }
    if (boot_ctx != 0) {
        boot_ctx->rsdp_linear = ACPI_RSDP_LINEAR;
        boot_ctx->acpi_pm1_evt = QEMU_ACPI_PM_BASE;
        boot_ctx->acpi_pm1_cnt = QEMU_ACPI_PM_BASE + 4u;
        boot_ctx->acpi_gpe0 = QEMU_ACPI_PM_BASE + 0x0cu;
        boot_ctx->acpi_gpe0_len = ACPI_GPE0_LEN;
        boot_ctx->acpi_flags = 0u;
    }
    serial_write_string("ACPI qemu tables ok\r\n");
}

__attribute__((section(".stage2.entry"), used)) void qemu_stage2_entry(
    unsigned int total_bytes, unsigned int aux_linear) {
    typedef void (*bios_entry_fn)(unsigned int, unsigned int);
    struct shared_service_table* service =
        shared_service_from_total(total_bytes);
    struct shared_boot_context* boot_ctx = shared_boot_context(service);
    struct shared_payload_entry* stage3_payload =
        shared_payload_find(service, SHARED_PAYLOAD_ID_STAGE3);
    const void* stage3_blob = 0;
    blob_expand_fn expand = 0;
    blob_load_fn load = 0;
    unsigned int blob_stage = 0u;
    unsigned int stage3_load = BIOS_LOAD_LINEAR;
    struct blob_status status;
    int rc;

    if (service != 0 && service->blob_expand != 0u) {
        expand = (blob_expand_fn)service->blob_expand;
        load = (blob_load_fn)service->blob_load;
        blob_stage = service->blob_stage;
    }
    if (stage3_payload != 0) {
        stage3_blob = (const void*)stage3_payload->blob_ptr;
    }

    zero_bss();
    serial_write_string("qemu stage2 @ 00080000\r\n");
    if (boot_ctx != 0) {
        boot_ctx->flags |= SHARED_BOOT_FLAG_SHADOW_READY;
    }
    install_qemu_acpi_tables(total_bytes, boot_ctx);

    if (load != 0) {
        rc = load(SHARED_PAYLOAD_ID_STAGE3, (void*)BIOS_LOAD_LINEAR,
                  BIOS_LOAD_CAPACITY, &stage3_load, &status, total_bytes);
    } else {
        if (stage3_blob == 0 || expand == 0 || blob_stage == 0u ||
            service->blob_stage_size < BLOB_STAGE_CAPACITY) {
            serial_write_string("qemu stage3 blob missing\r\n");
            outb(0x80u, 0xefu);
            for (;;) {
                __asm__ volatile("hlt");
            }
        }
        stage3_load = blob_load_addr_or(stage3_blob, BIOS_LOAD_LINEAR);
        rc = expand(stage3_blob, (void*)blob_stage, (void*)stage3_load,
                    BIOS_LOAD_CAPACITY, &status, total_bytes);
    }
    if (rc != 0) {
        serial_write_string("\r\nqemu stage3 load failed rc=");
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
    serial_write_string("\r\nqemu stage3 copied\r\n");
    ((bios_entry_fn)stage3_load)(total_bytes, aux_linear);
    for (;;) {
        __asm__ volatile("hlt");
    }
}
