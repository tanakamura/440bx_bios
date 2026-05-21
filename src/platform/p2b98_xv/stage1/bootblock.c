#include "post_code.h"
#include "blob.h"
#include "shared_service/service_table.h"

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

#define FAST_A20_PORT 0x0092
#define FAST_A20_ENABLE 0x02
#define FAST_RESET_BIT 0x01

#define BX_DRAMC 0x57
#define BX_DRAMT 0x58
#define BX_FDHC 0x68
#define BX_MBSC 0x69
#define BX_MBFS 0xca
#define BX_DRB0 0x60
#define BX_RPS 0x74
#define BX_SDRAMC 0x76
#define BX_PGPOL 0x78
#define BX_PMCR 0x7a
#define BX_NBXCFG 0x50

#define SMBHSTSTS 0
#define SMBHSTCNT 2
#define SMBHSTCMD 3
#define SMBHSTADD 4
#define SMBHSTDAT0 5

#define PIIX4_BYTE_DATA 0x08
#define SMBHSTCNT_START 0x40
#define SMBHSTSTS_HOST_BUSY 0x01
#define SMBHSTSTS_DEV_ERR 0x04
#define SMBHSTSTS_BUS_ERR 0x08
#define SMBHSTSTS_FAILED 0x10
#define ROM_HIGH_DELTA 0xfff00000u

extern void postcar_transition(unsigned int stack_top, unsigned int mtrr_mask,
                               unsigned int total_bytes, unsigned int gdtr_ptr);
extern unsigned char __blob_service_start[];
extern unsigned char __blob_service_end[];
extern int blob_expand_service(const void* blob, void* stage, void* dst,
                               unsigned int dst_capacity,
                               struct blob_status* status,
                               unsigned int total_bytes);
extern int blob_load_service(unsigned int payload_id, void* fallback_dst,
                             unsigned int dst_capacity,
                             unsigned int* load_addr_out,
                             struct blob_status* status,
                             unsigned int total_bytes);
extern void* shared_heap_alloc_service(unsigned int total_bytes,
                                       unsigned int size);
extern void shared_heap_free_service(unsigned int total_bytes, void* ptr);
extern void* shared_heap_realloc_service(unsigned int total_bytes, void* ptr,
                                         unsigned int size);

static void serial_write_char(char c) {
    while ((inb(0x03f8 + 5) & 0x20) == 0) {
    }
    outb(0x03f8, (unsigned char)c);
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

static void die_with_post(unsigned char code) {
    outb(0x80, code);
    for (;;) {
        __asm__ volatile("hlt");
    }
}

static unsigned int pci_read32(unsigned char bus, unsigned char device,
                               unsigned char function, unsigned char reg) {
    unsigned int address = 0x80000000u | ((unsigned int)bus << 16) |
                           ((unsigned int)device << 11) |
                           ((unsigned int)function << 8) | (reg & 0xfcu);
    outl(0x0cf8, address);
    return inl(0x0cfc);
}

static unsigned short pci_read16(unsigned char bus, unsigned char device,
                                 unsigned char function, unsigned char reg) {
    unsigned int value = pci_read32(bus, device, function, reg);
    if (reg & 0x02) {
        return (unsigned short)(value >> 16);
    }
    return (unsigned short)value;
}

static unsigned char pci_read8(unsigned char bus, unsigned char device,
                               unsigned char function, unsigned char reg) {
    unsigned int value = pci_read32(bus, device, function, reg);
    return (unsigned char)(value >> ((reg & 0x03) * 8));
}

static void pci_write16(unsigned char bus, unsigned char device,
                        unsigned char function, unsigned char reg,
                        unsigned short val) {
    unsigned char reg_lo = reg & 0x02;
    unsigned int address = 0x80000000u | ((unsigned int)bus << 16) |
                           ((unsigned int)device << 11) |
                           ((unsigned int)function << 8) | (reg & 0xfcu);
    outl(0x0cf8, address);
    outw((unsigned short)(0x0cfc + reg_lo), val);
}

static void pci_write8(unsigned char bus, unsigned char device,
                       unsigned char function, unsigned char reg,
                       unsigned char val) {
    unsigned char reg_lo = reg & 0x03;
    unsigned int address = 0x80000000u | ((unsigned int)bus << 16) |
                           ((unsigned int)device << 11) |
                           ((unsigned int)function << 8) | (reg & 0xfcu);
    outl(0x0cf8, address);
    outb((unsigned short)(0x0cfc + reg_lo), val);
}

static const unsigned char* rom_high_ptr(const unsigned char* ptr) {
    return (const unsigned char*)((unsigned int)ptr + ROM_HIGH_DELTA);
}

static unsigned char rom_read_stable_u8(const unsigned char* ptr) {
    volatile const unsigned char* p = (volatile const unsigned char*)ptr;
    unsigned char a = p[0];
    unsigned char b = p[0];
    unsigned char c;
    unsigned char d;
    unsigned char e;

    if (a == b) {
        return a;
    }
    c = p[0];
    if (c == a || c == b) {
        return c;
    }
    d = p[0];
    if (d == a || d == b || d == c) {
        return d;
    }
    e = p[0];
    if (e == a || e == b || e == c || e == d) {
        return e;
    }
    return e;
}

static void enable_extended_bios_decode(void) {
    unsigned short xbcs = pci_read16(0, 7, 0, 0x4e);
    xbcs |= (1u << 9) | (1u << 7) | (1u << 6) | (1u << 2);
    pci_write16(0, 7, 0, 0x4e, xbcs);
}

static int smbus_wait_ready(unsigned short smba) {
    unsigned int timeout = 0;
    while ((inb(smba + SMBHSTSTS) & SMBHSTSTS_HOST_BUSY) != 0) {
        ++timeout;
        if (timeout > 1000000u) {
            return -1;
        }
    }
    return 0;
}

static int smbus_read_byte_data(unsigned short smba, unsigned char addr,
                                unsigned char command,
                                unsigned char* out_value) {
    unsigned char status;
    unsigned int timeout;

    status = inb(smba + SMBHSTSTS);
    if (status != 0) {
        outb(smba + SMBHSTSTS, status);
        status = inb(smba + SMBHSTSTS);
        if (status != 0) {
            return 0xe0u | status;
        }
    }

    if (smbus_wait_ready(smba) != 0) {
        return 0xf1;
    }

    outb(smba + SMBHSTADD, (unsigned char)((addr << 1) | 1));
    outb(smba + SMBHSTCMD, command);
    outb(smba + SMBHSTCNT, PIIX4_BYTE_DATA);
    outb(smba + SMBHSTCNT, PIIX4_BYTE_DATA | SMBHSTCNT_START);

    timeout = 0;
    do {
        status = inb(smba + SMBHSTSTS);
        ++timeout;
        if (timeout > 1000000u) {
            return 0xf2;
        }
    } while ((status & SMBHSTSTS_HOST_BUSY) != 0);

    if ((status & (SMBHSTSTS_DEV_ERR | SMBHSTSTS_BUS_ERR | SMBHSTSTS_FAILED)) !=
        0) {
        outb(smba + SMBHSTSTS, status);
        return status;
    }

    *out_value = inb(smba + SMBHSTDAT0);
    if (status != 0) {
        outb(smba + SMBHSTSTS, status);
    }
    return 0;
}

static int probe_spd_device(unsigned short smba, unsigned char addr) {
    unsigned char value = 0;
    return smbus_read_byte_data(smba, addr, 0, &value);
}

static int read_spd_minimal(unsigned short smba, unsigned char addr,
                            unsigned char* out) {
    static const unsigned char needed[] = {2, 3, 4, 5, 6, 7, 17, 18};
    unsigned int i;

    for (i = 0; i < sizeof(needed); ++i) {
        int rc = smbus_read_byte_data(smba, addr, needed[i], &out[needed[i]]);
        if (rc != 0) {
            return rc;
        }
    }
    return 0;
}

static void enable_a20_fast(void) {
    unsigned char v = inb(FAST_A20_PORT);
    v |= FAST_A20_ENABLE;
    v &= (unsigned char)~FAST_RESET_BIT;
    outb(FAST_A20_PORT, v);
}

static int a20_alias_test(void) {
    volatile unsigned int* low = (volatile unsigned int*)0x00080000u;
    volatile unsigned int* high = (volatile unsigned int*)0x00180000u;
    unsigned int save_low = *low;
    unsigned int save_high = *high;
    unsigned int v0 = 0x13579bdfu;
    unsigned int v1 = 0x2468ace0u;
    int ok;

    *low = v0;
    *high = v1;
    ok = (*low == v0) && (*high == v1);
    *low = save_low;
    *high = save_high;
    return ok ? 0 : -1;
}

static unsigned int largest_power_of_two(unsigned int value) {
    unsigned int bit = 1u;
    while (bit <= (value >> 1)) {
        bit <<= 1;
    }
    return bit;
}

static unsigned int dram_mtrr_mask(unsigned int total_bytes) {
    unsigned int size = largest_power_of_two(total_bytes);
    return (~(size - 1u) & 0xfffff000u) | 0x00000800u;
}

static unsigned int blob_service_size_aligned(void) {
    return shared_align_up(
        (unsigned int)(__blob_service_end - __blob_service_start), 16u);
}

static unsigned int blob_service_base_for_total(unsigned int total_bytes) {
    return shared_service_base_from_table(
        shared_table_base_from_total(total_bytes), blob_service_size_aligned());
}

static unsigned int dram_stack_top(unsigned int total_bytes) {
    if (total_bytes >= 0x00300000u) {
        return blob_service_base_for_total(total_bytes);
    }
    return 0x001ff000u;
}

static int dram_test_rw(void) {
    volatile unsigned int* p0 = (volatile unsigned int*)0x00100000u;
    volatile unsigned int* p1 = (volatile unsigned int*)0x00100004u;

    *p0 = 0x11223344u;
    *p1 = 0x55667788u;
    if (*p0 != 0x11223344u || *p1 != 0x55667788u) {
        return -1;
    }

    *p0 = 0xa5a55a5au;
    *p1 = 0x5a5aa5a5u;
    if (*p0 != 0xa5a55a5au || *p1 != 0x5a5aa5a5u) {
        return -1;
    }
    return 0;
}

static unsigned short dram_mode_reg_address(unsigned char cas_latency,
                                            unsigned char start_row) {
    if (start_row >= 4) {
        if (cas_latency == 2) {
            return 0x1ea8;
        }
        return 0x1e28;
    }
    if (cas_latency == 2) {
        return 0x0150;
    }
    return 0x01d0;
}

static int dram_init_from_spd(const unsigned char* spd, unsigned char spd_slot,
                              unsigned int* total_bytes_out) {
    unsigned char row_bits = spd[3];
    unsigned char col_bits = spd[4];
    unsigned char module_banks = spd[5];
    unsigned short data_width =
        (unsigned short)(spd[6] | ((unsigned short)spd[7] << 8));
    unsigned char internal_banks = spd[17];
    unsigned char cas_mask = spd[18];
    unsigned char row;
    unsigned char drb_val;
    unsigned char page_code;
    unsigned char start_row;
    unsigned char end_row;
    unsigned short rps = 0;
    unsigned short pgpol = 0x0008;
    unsigned char dramc57;
    unsigned short sdramc_normal;
    unsigned char cas_latency = 3;
    unsigned int row_size_bytes;
    unsigned short mode_addr;
    unsigned char pmcr;
    unsigned char mbsc0;
    unsigned short mbsc1;
    unsigned short mbsc3;
    unsigned char mbfs0;
    unsigned char mbfs1;
    unsigned char mbfs2;

    if (spd[2] != 0x04 || data_width != 64 || module_banks == 0 ||
        module_banks > 2 || row_bits < 11 || row_bits > 13 || col_bits < 8 ||
        col_bits > 11 || internal_banks != 4) {
        return -1;
    }
    if (spd_slot < 0x50 || spd_slot > 0x52) {
        return -1;
    }

    start_row = (unsigned char)((spd_slot - 0x50) * 2);
    end_row = (unsigned char)(start_row + module_banks);
    if (end_row > 6) {
        return -1;
    }

    row_size_bytes = (1u << row_bits) * (1u << col_bits) * internal_banks *
                     (data_width / 8u);
    if ((row_size_bytes % (8u * 1024u * 1024u)) != 0) {
        return -1;
    }
    drb_val = (unsigned char)(row_size_bytes / (8u * 1024u * 1024u));
    if (drb_val == 0) {
        return -1;
    }

    if (cas_mask & 0x04) {
        cas_latency = 3;
    } else if (cas_mask & 0x02) {
        cas_latency = 2;
    } else {
        return -1;
    }

    switch (((unsigned int)(1u << col_bits) * data_width) / 8u) {
        case 2048:
            page_code = 0;
            break;
        case 4096:
            page_code = 1;
            break;
        case 8192:
            page_code = 2;
            break;
        default:
            return -1;
    }

    pci_write8(0, 0, 0, BX_NBXCFG + 3, 0xff);
    pci_write8(0, 0, 0, BX_FDHC, 0x00);

    mbsc0 = 0xaa;
    mbsc1 = 0xafea;
    mbsc3 = 0xb00a;
    mbfs0 = 0x00;
    mbfs1 = 0x00;
    mbfs2 = 0x1e;

    if (start_row < 4 && end_row > 2) {
        mbsc1 |= 0x003c;
        mbfs2 |= 0x40;
    } else {
        mbsc3 |= 0xc000;
    }
    if (end_row > 4) {
        mbsc0 |= 0x30;
        mbfs0 |= 0x02;
    }

    pci_write8(0, 0, 0, BX_MBSC + 0, mbsc0);
    pci_write16(0, 0, 0, BX_MBSC + 1, mbsc1);
    pci_write16(0, 0, 0, BX_MBSC + 3, mbsc3);
    pci_write16(0, 0, 0, BX_MBFS + 0, (unsigned short)((mbfs1 << 8) | mbfs0));
    pci_write8(0, 0, 0, BX_MBFS + 2, mbfs2);

    for (row = 0; row < 8; ++row) {
        unsigned char accum = 0;
        if (row >= start_row) {
            unsigned char populated = (row < end_row)
                                          ? (unsigned char)(row - start_row + 1)
                                          : module_banks;
            accum = (unsigned char)(populated * drb_val);
        }
        pci_write8(0, 0, 0, (unsigned char)(BX_DRB0 + row), accum);
        if (row >= start_row && row < end_row) {
            rps |= (unsigned short)(page_code << (row * 2));
            pgpol |= (unsigned short)(1u << (8 + row));
        }
    }

    pci_write16(0, 0, 0, BX_RPS, rps);
    pci_write16(0, 0, 0, BX_PGPOL, pgpol);

    dramc57 = pci_read8(0, 0, 0, BX_DRAMC);
    dramc57 &= 0x20;
    dramc57 |= 0x08;
    pci_write8(0, 0, 0, BX_DRAMC, dramc57);
    pci_write8(0, 0, 0, BX_DRAMT, 0x03);

    sdramc_normal = 0x0103;
    if (cas_latency == 2) {
        sdramc_normal |= 0x0004;
    }

    mode_addr = dram_mode_reg_address(cas_latency, start_row);

    serial_write_string("DramSeq...");
    __asm__ volatile(
        "movl $0x80000074, %%ebx\n\t"
        "xorl %%edi, %%edi\n\t"
        "movl %%ebx, %%eax\n\t"
        "movw $0x0cf8, %%dx\n\t"
        "outl %%eax, %%dx\n\t"
        "movw $0x0cfe, %%dx\n\t"
        "movw %[nop], %%ax\n\t"
        "outw %%ax, %%dx\n\t"
        "movl $0, (%%edi)\n\t"
        "movl $1500, %%ecx\n\t"
        "1: loop 1b\n\t"
        "movl %%ebx, %%eax\n\t"
        "movw $0x0cf8, %%dx\n\t"
        "outl %%eax, %%dx\n\t"
        "movw $0x0cfe, %%dx\n\t"
        "movw %[pre], %%ax\n\t"
        "outw %%ax, %%dx\n\t"
        "movl $0, (%%edi)\n\t"
        "movl $64, %%ecx\n\t"
        "2: loop 2b\n\t"
        "movl $8, %%esi\n\t"
        "3:\n\t"
        "movl %%ebx, %%eax\n\t"
        "movw $0x0cf8, %%dx\n\t"
        "outl %%eax, %%dx\n\t"
        "movw $0x0cfe, %%dx\n\t"
        "movw %[cbr], %%ax\n\t"
        "outw %%ax, %%dx\n\t"
        "movl $0, (%%edi)\n\t"
        "movl $64, %%ecx\n\t"
        "4: loop 4b\n\t"
        "decl %%esi\n\t"
        "jnz 3b\n\t"
        "movl %%ebx, %%eax\n\t"
        "movw $0x0cf8, %%dx\n\t"
        "outl %%eax, %%dx\n\t"
        "movw $0x0cfe, %%dx\n\t"
        "movw %[mrs], %%ax\n\t"
        "outw %%ax, %%dx\n\t"
        "movl %[mode], %%edi\n\t"
        "movl $0, (%%edi)\n\t"
        "movl $64, %%ecx\n\t"
        "5: loop 5b\n\t"
        :
        : [nop] "rm"((unsigned short)(sdramc_normal | 0x0020)),
          [pre] "rm"((unsigned short)(sdramc_normal | 0x0040)),
          [cbr] "rm"((unsigned short)(sdramc_normal | 0x0080)),
          [mrs] "rm"((unsigned short)(sdramc_normal | 0x0060)),
          [mode] "rm"((unsigned int)mode_addr)
        : "eax", "ebx", "ecx", "edx", "esi", "edi", "memory", "cc");
    serial_write_string(" done");

    pci_write16(0, 0, 0, BX_SDRAMC, sdramc_normal);
    dramc57 = (unsigned char)((dramc57 & 0x20) | 0x09);
    pci_write8(0, 0, 0, BX_DRAMC, dramc57);
    pmcr = pci_read8(0, 0, 0, BX_PMCR);
    pmcr |= 0x10;
    pci_write8(0, 0, 0, BX_PMCR, pmcr);
    serial_write_string(" OK\r\n");

    *total_bytes_out = row_size_bytes * module_banks;
    return 0;
}

static void install_blob_service(unsigned int service_base) {
    const unsigned char* src = rom_high_ptr(__blob_service_start);
    unsigned char* dst = (unsigned char*)service_base;
    unsigned int size =
        (unsigned int)(__blob_service_end - __blob_service_start);
    unsigned int i;

    for (i = 0; i < size; ++i) {
        dst[i] = rom_read_stable_u8(src + i);
    }
    __asm__ volatile("xorl %%eax, %%eax\n\tcpuid"
                     :
                     :
                     : "eax", "ebx", "ecx", "edx", "memory");
}

static void install_shared_service_table(unsigned int total_bytes,
                                         unsigned int stack_top,
                                         unsigned int service_base) {
    unsigned int table_linear = shared_table_base_from_total(total_bytes);
    unsigned int ptr_slot = shared_table_pointer_slot(total_bytes);
    struct shared_service_table* table =
        (struct shared_service_table*)table_linear;
    struct shared_payload_manifest* manifest;
    struct shared_boot_context* ctx;
    unsigned char* heap_base;
    unsigned int i;

    for (i = 0; i < SHARED_TABLE_BYTES; ++i) {
        ((volatile unsigned char*)table_linear)[i] = 0u;
    }

    manifest = (struct shared_payload_manifest*)shared_align_up(
        table_linear + sizeof(*table), 16u);
    ctx = (struct shared_boot_context*)shared_align_up(
        (unsigned int)(manifest + 1), 16u);
    heap_base = (unsigned char*)shared_align_up((unsigned int)(ctx + 1), 16u);

    table->magic = SHARED_SERVICE_MAGIC;
    table->version = SHARED_SERVICE_VERSION;
    table->size = sizeof(*table);
    table->total_dram_bytes = total_bytes;
    table->service_base = service_base;
    table->service_size =
        (unsigned int)(__blob_service_end - __blob_service_start);
    table->table_linear = table_linear;
    table->table_size = SHARED_TABLE_BYTES;
    table->stack_top = stack_top;
    table->heap_base = (unsigned int)heap_base;
    table->heap_limit = ptr_slot;
    shared_heap_init(table);
    table->boot_context_ptr = (unsigned int)ctx;
    table->payload_manifest_ptr = (unsigned int)manifest;
    table->blob_expand =
        service_base +
        ((unsigned int)blob_expand_service - (unsigned int)__blob_service_start);
    table->blob_stage = 0u;
    table->blob_stage_size = 0u;
    table->blob_load =
        service_base +
        ((unsigned int)blob_load_service - (unsigned int)__blob_service_start);
    table->heap_alloc =
        service_base +
        ((unsigned int)shared_heap_alloc_service -
         (unsigned int)__blob_service_start);
    table->heap_free =
        service_base +
        ((unsigned int)shared_heap_free_service -
         (unsigned int)__blob_service_start);
    table->heap_realloc =
        service_base +
        ((unsigned int)shared_heap_realloc_service -
         (unsigned int)__blob_service_start);

    if (shared_payload_manifest_from_rom_directory(manifest, 0xfffc0000u) !=
        0) {
        manifest->magic = SHARED_PAYLOAD_MAGIC;
        manifest->version = SHARED_PAYLOAD_VERSION;
        manifest->entry_count = 0u;
        manifest->reserved = 0u;
    }

    ctx->magic = SHARED_BOOT_CONTEXT_MAGIC;
    ctx->version = SHARED_BOOT_CONTEXT_VERSION;
    ctx->size = sizeof(*ctx);
    ctx->total_dram_bytes = total_bytes;
    ctx->flags = SHARED_BOOT_FLAG_PLATFORM_P2B98_XV;
    ctx->platform_id = SHARED_BOOT_FLAG_PLATFORM_P2B98_XV;
    {
        struct shared_payload_entry* dsdt_payload =
            shared_payload_find(table, SHARED_PAYLOAD_ID_DSDT);
        if (dsdt_payload != 0) {
            ctx->acpi_input_ptr = dsdt_payload->blob_ptr;
            ctx->acpi_input_size = dsdt_payload->blob_size;
        }
    }

    *(volatile unsigned int*)ptr_slot = (unsigned int)table;
}

static void enter_stage2(unsigned int total_bytes) {
    typedef void (*stage2_entry_fn)(unsigned int, unsigned int);
    struct blob_status status;
    struct shared_service_table* service =
        shared_service_from_total(total_bytes);
    blob_load_fn load = 0;
    unsigned int stage2_load = STAGE2_LOAD_LINEAR;
    int rc;

    if (service != 0) {
        load = (blob_load_fn)service->blob_load;
    }

    if (load != 0) {
        rc = load(SHARED_PAYLOAD_ID_STAGE2, (void*)STAGE2_LOAD_LINEAR,
                  STAGE2_LOAD_CAPACITY, &stage2_load, &status, total_bytes);
    } else {
        serial_write_string("blobsvc missing\r\n");
        die_with_post(0xef);
    }
    if (rc != 0) {
        serial_write_string("\r\nstage2 load failed rc=");
        serial_write_hex8((unsigned char)rc);
        serial_write_string(" block=");
        serial_write_hex32(status.block);
        serial_write_string(" exp=");
        serial_write_hex32(status.expected);
        serial_write_string(" got=");
        serial_write_hex32(status.got);
        serial_write_string("\r\n");
        die_with_post(0xef);
    }

    serial_write_string("\r\nstage2 copied\r\n");
    ((stage2_entry_fn)stage2_load)(total_bytes, 0u);
    die_with_post(0xef);
}

static unsigned int prepare_runtime_gdt(void) {
    static const unsigned long long gdt_template[] = {
        0x0000000000000000ull, 0x00cf9b000000ffffull, 0x00cf93000000ffffull,
        0x00009b0fe000ffffull, 0x0000930fe000ffffull,
    };
    volatile unsigned char* gdtr = (volatile unsigned char*)0x0007f100u;
    volatile unsigned long long* gdt =
        (volatile unsigned long long*)0x0007f108u;
    unsigned int i;

    gdtr[0] = (unsigned char)((sizeof(gdt_template) - 1u) & 0xffu);
    gdtr[1] = (unsigned char)(((sizeof(gdt_template) - 1u) >> 8) & 0xffu);

    for (i = 0; i < (sizeof(gdt_template) / sizeof(gdt_template[0])); ++i) {
        gdt[i] = gdt_template[i];
    }

    {
        unsigned int base = 0x0007f108u;
        gdtr[2] = (unsigned char)(base & 0xffu);
        gdtr[3] = (unsigned char)((base >> 8) & 0xffu);
        gdtr[4] = (unsigned char)((base >> 16) & 0xffu);
        gdtr[5] = (unsigned char)((base >> 24) & 0xffu);
    }

    return 0x0007f100u;
}

void postcar_bootblock_resume(unsigned int total_bytes) {
    unsigned int stack_top = dram_stack_top(total_bytes);
    unsigned int service_base = blob_service_base_for_total(total_bytes);

    serial_write_string("bootblock post-CAR\r\n");
    serial_write_string("Install blobsvc...\r\n");
    install_blob_service(service_base);
    serial_write_string("blobsvc ok\r\n");
    install_shared_service_table(total_bytes, stack_top, service_base);
    serial_write_string("svctab ok\r\n");
    serial_write_string("Load stage2 @ 00080000...\r\n");
    enter_stage2(total_bytes);
    for (;;) {
        __asm__ volatile("hlt");
    }
}

void c_entry(void) {
    unsigned int total_bytes = 0;
    unsigned int wb_mask;
    unsigned int new_stack_top;
    unsigned int runtime_gdtr;
    unsigned short smba;
    unsigned short pci_cmd;
    unsigned short hostcfg;
    unsigned char slot;
    int probe_rc;
    unsigned char spd[64] = {0};
    unsigned char found_spd = 0;

    outb(0x80, POST_ENTER_C);
    serial_write_string("bootblock\r\n");

    enable_extended_bios_decode();

    pci_cmd = pci_read16(0, 7, 3, 0x04);
    if ((pci_cmd & 0x0001u) == 0) {
        pci_write16(0, 7, 3, 0x04, (unsigned short)(pci_cmd | 0x0001u));
    }

    outb(0x80, POST_SMBUS_PROBE_START);
    smba = (unsigned short)(pci_read16(0, 7, 3, 0x90) & 0xfff0u);
    hostcfg = pci_read16(0, 7, 3, 0xd2);

    if (smba == 0) {
        pci_write16(0, 7, 3, 0x90, 0x1000);
        smba = (unsigned short)(pci_read16(0, 7, 3, 0x90) & 0xfff0u);
    }
    if ((hostcfg & 0x0001u) == 0) {
        hostcfg |= 0x0001u;
        pci_write16(0, 7, 3, 0xd2, hostcfg);
    }

    enable_a20_fast();

    for (slot = 0x50; slot <= 0x53; ++slot) {
        probe_rc = probe_spd_device(smba, slot);
        if (probe_rc != 0) {
            serial_write_string("SPD ");
            serial_write_hex8(slot);
            serial_write_string(": !");
            serial_write_hex8((unsigned char)probe_rc);
            serial_write_string("\r\n");
            continue;
        }
        found_spd = slot;
        serial_write_string("SPD ");
        serial_write_hex8(slot);
        serial_write_string(": ok\r\n");
        if (read_spd_minimal(smba, slot, spd) != 0) {
            die_with_post(POST_DRAM_INIT_FAIL);
        }
        break;
    }

    if (found_spd == 0) {
        die_with_post(POST_DRAM_INIT_FAIL);
    }

    outb(0x80, POST_DRAM_INIT_START);
    serial_write_string("DRAM init...\r\n");
    if (dram_init_from_spd(spd, found_spd, &total_bytes) != 0) {
        die_with_post(POST_DRAM_UNSUPPORTED);
    }
    serial_write_string("DRAM init ok\r\n");

    if (a20_alias_test() != 0) {
        die_with_post(0xf5);
    }
    serial_write_string("A20 ok\r\n");

    if (dram_test_rw() != 0) {
        die_with_post(POST_DRAM_TEST_FAIL);
    }
    outb(0x80, POST_DRAM_TEST_PASS);
    serial_write_string("DRAM test ok\r\n");

    outb(0x80, POST_LEAVE_CAR);
    serial_write_string("Leaving CAR...\r\n");
    wb_mask = dram_mtrr_mask(total_bytes);
    new_stack_top = dram_stack_top(total_bytes);

    runtime_gdtr = prepare_runtime_gdt();
    postcar_transition(new_stack_top, wb_mask, total_bytes, runtime_gdtr);
    for (;;) {
    }
}
