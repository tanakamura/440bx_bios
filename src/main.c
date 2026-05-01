#include "post_code.h"

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

static inline unsigned short inw(unsigned short port) {
    unsigned short value;
    __asm__ volatile("inw %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}
static inline void outl(unsigned short port, unsigned int value) {
    __asm__ volatile("outl %0, %1" : : "a"(value), "Nd"(port));
}

static inline unsigned int inl(unsigned short port) {
    unsigned int value;
    __asm__ volatile("inl %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

#define COM1_BASE 0x03f8
#define UART_LCR 3
#define UART_LSR 5
#define UART_SCR 7

#define BX_DRAMC 0x57
#define BX_DRAMT 0x58
#define BX_PAM0 0x59
#define BX_DRB0 0x60
#define BX_FDHC 0x68
#define BX_MBSC 0x69
#define BX_MBFS 0xca
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

extern void postcar_transition(unsigned int stack_top, unsigned int mtrr_mask,
                               unsigned int total_bytes);

static unsigned int pci_read32(unsigned char bus, unsigned char device,
                               unsigned char function, unsigned char reg) {
    unsigned int address = 0x80000000u | ((unsigned int)bus << 16) |
                           ((unsigned int)device << 11) |
                           ((unsigned int)function << 8) | (reg & 0xfcu);
    outl(0x0cf8, address);
    return inl(0x0cfc);
}
static void pci_write32(unsigned char bus, unsigned char device,
                        unsigned char function, unsigned char reg,
                        unsigned int val) {
    unsigned int address = 0x80000000u | ((unsigned int)bus << 16) |
                           ((unsigned int)device << 11) |
                           ((unsigned int)function << 8) | (reg & 0xfcu);
    outl(0x0cf8, address);
    outl(0x0cfc, val);
}
static void pci_write16(unsigned char bus, unsigned char device,
                        unsigned char function, unsigned char reg,
                        unsigned short val) {
    unsigned char reg_lo = reg & 0x02;

    unsigned int address = 0x80000000u | ((unsigned int)bus << 16) |
                           ((unsigned int)device << 11) |
                           ((unsigned int)function << 8) | (reg & 0xfcu);
    outl(0x0cf8, address);
    outw(0x0cfc + reg_lo, val);
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

static unsigned int dram_stack_top(unsigned int total_bytes) {
    unsigned int top = 0x00200000u;

    if (total_bytes > 0x00110000u && top >= total_bytes) {
        top = (total_bytes & ~0xfffu) - 0x1000u;
    }
    if (top < 0x00110000u) {
        top = 0x00110000u;
    }
    return top;
}

static unsigned int tsc_low(void) {
    unsigned int value;
    __asm__ volatile("rdtsc" : "=a"(value) : : "edx");
    return value;
}

static unsigned long long rdmsr(unsigned int msr) {
    unsigned int lo;
    unsigned int hi;
    __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((unsigned long long)hi << 32) | lo;
}

static void wrmsr64(unsigned int msr, unsigned int lo, unsigned int hi) {
    __asm__ volatile("wrmsr" : : "c"(msr), "a"(lo), "d"(hi));
}

static unsigned int read_cr0(void) {
    unsigned int value;
    __asm__ volatile("mov %%cr0, %0" : "=r"(value));
    return value;
}

static void write_cr0(unsigned int value) {
    __asm__ volatile("mov %0, %%cr0" : : "r"(value) : "memory");
}

static void die_with_post(unsigned char code) {
    outb(0x80, code);
    for (;;) {
        __asm__ volatile("hlt");
    }
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

static int read_spd_block(unsigned short smba, unsigned char addr,
                          unsigned char* out, unsigned char length) {
    unsigned char offset;
    for (offset = 0; offset < length; ++offset) {
        int rc = smbus_read_byte_data(smba, addr, offset, &out[offset]);
        if (rc != 0) {
            return rc;
        }
    }
    return 0;
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

static void print_spd_dump(unsigned short smba, unsigned char addr,
                           unsigned char length) {
    unsigned char offset;

    serial_write_string("SPD ");
    serial_write_hex8(addr);
    serial_write_string(":");

    for (offset = 0; offset < length; ++offset) {
        unsigned char value = 0;
        int rc = smbus_read_byte_data(smba, addr, offset, &value);
        if ((offset & 0x0f) == 0) {
            serial_write_string("\r\n  ");
            serial_write_hex8(offset);
            serial_write_char(':');
        }
        serial_write_char(' ');
        if (rc != 0) {
            serial_write_char('!');
            serial_write_hex8((unsigned char)rc);
            break;
        }
        serial_write_hex8(value);
    }
    serial_write_string("\r\n");
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
    unsigned short pgpol = 0x0008; /* infinite idle timer */
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

    /* Match the no-ECC assumption for the populated rows. */
    pci_write8(0, 0, 0, BX_NBXCFG + 3, 0xff);

    /*
     * Do not switch C/D/E/F segments to DRAM yet.
     * We are still executing from the reset ROM window at F0000h, so remapping
     * PAM this early can redirect instruction fetches into uninitialized DRAM.
     */

    /* No DRAM holes. */
    pci_write8(0, 0, 0, BX_FDHC, 0x00);

    /* 3-DIMM board defaults derived from coreboot's ASUS P2B path. */
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
    dramc57 &= 0x20; /* preserve MMCONFIG only */
    dramc57 |= 0x08; /* SDRAM + refresh disabled during init */
    pci_write8(0, 0, 0, BX_DRAMC, dramc57);
    pci_write8(0, 0, 0, BX_DRAMT, 0x03);

    sdramc_normal = 0x0103; /* IPDLT=01, CL3, tRCD=2, tRP=2, LCT=0 */
    if (cas_latency == 2) {
        sdramc_normal |= 0x0004;
    }

    mode_addr = dram_mode_reg_address(cas_latency, start_row);

    serial_write_string("DramSeq...");
    __asm__ volatile(
        "movl $0x80000074, %%ebx\n\t"
        "xorl %%edi, %%edi\n\t"

        /* NOP */
        "movl %%ebx, %%eax\n\t"
        "movw $0x0cf8, %%dx\n\t"
        "outl %%eax, %%dx\n\t"
        "movw $0x0cfe, %%dx\n\t"
        "movw %[nop], %%ax\n\t"
        "outw %%ax, %%dx\n\t"
        "movl $0, (%%edi)\n\t"
        "movl $1500, %%ecx\n\t"
        "1: loop 1b\n\t"

        /* PRECHARGE ALL */
        "movl %%ebx, %%eax\n\t"
        "movw $0x0cf8, %%dx\n\t"
        "outl %%eax, %%dx\n\t"
        "movw $0x0cfe, %%dx\n\t"
        "movw %[pre], %%ax\n\t"
        "outw %%ax, %%dx\n\t"
        "movl $0, (%%edi)\n\t"
        "movl $64, %%ecx\n\t"
        "2: loop 2b\n\t"

        /* 8x CBR refresh */
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

        /* Mode register set */
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
    dramc57 =
        (unsigned char)((dramc57 & 0x20) | 0x09); /* SDRAM + 15.6us refresh */
    pci_write8(0, 0, 0, BX_DRAMC, dramc57);
    pmcr = pci_read8(0, 0, 0, BX_PMCR);
    pmcr |= 0x10; /* NREF_EN */
    pci_write8(0, 0, 0, BX_PMCR, pmcr);
    serial_write_string(" OK\r\n");
    *total_bytes_out = row_size_bytes * module_banks;

    return 0;
}

static int dram_test_rw(void) {
    volatile unsigned int* p0 = (volatile unsigned int*)0x00100000u;
    volatile unsigned int* p1 = (volatile unsigned int*)0x00100004u;
    unsigned int v0;
    unsigned int v1;

    *p0 = 0x11223344u;
    *p1 = 0x55667788u;

    v0 = *p0;
    v1 = *p1;

    if (v0 != 0x11223344u) {
        return -1;
    }
    if (v1 != 0x55667788u) {
        return -1;
    }

    *p0 = 0xa5a55a5au;
    *p1 = 0x5a5aa5a5u;

    v0 = *p0;
    v1 = *p1;

    if (v0 != 0xa5a55a5au) {
        return -1;
    }
    if (v1 != 0x5a5aa5a5u) {
        return -1;
    }

    return 0;
}

static int dram_count_test(unsigned int total_bytes) {
    unsigned int addr;
    unsigned int shown_kb = 0;

    serial_write_string("Memory Test: ");
    for (addr = 0; addr < total_bytes; addr += 0x1000u) {
        if (addr >= 0x000a0000u && addr < 0x00100000u) {
            addr = 0x00100000u - 0x1000u;
            continue;
        }
        volatile unsigned int* p = (volatile unsigned int*)addr;
        unsigned int save = *p;
        unsigned int pattern = 0xa5a50000u ^ addr;

        *p = pattern;
        if (*p != pattern) {
            return -1;
        }
        *p = save;
        if (*p != save) {
            return -1;
        }

        if (((addr >> 10) - shown_kb) >= 1024u) {
            shown_kb = addr >> 10;
            serial_write_char('\r');
            serial_write_string("Memory Test: ");
            serial_write_u32(shown_kb);
            serial_write_string("K");
        }
    }

    serial_write_char('\r');
    serial_write_string("Memory Test: ");
    serial_write_u32(total_bytes >> 10);
    serial_write_string("K OK\r\n");
    return 0;
}

static unsigned int bandwidth_read_x100(volatile unsigned int* base,
                                        unsigned int bytes, unsigned int passes,
                                        volatile unsigned int* checksum_out) {
    unsigned int start;
    unsigned int end;
    unsigned int sum = 0;
    unsigned int pass;
    unsigned int offset;
    unsigned int words = bytes / 4u;
    unsigned int limit = words & ~7u;
    unsigned int tail = limit;
    unsigned int s0 = 0;
    unsigned int s1 = 0;
    unsigned int s2 = 0;
    unsigned int s3 = 0;
    unsigned int s4 = 0;
    unsigned int s5 = 0;
    unsigned int s6 = 0;
    unsigned int s7 = 0;

    for (offset = 0; offset < words; ++offset) {
        base[offset] = (0x13579bdfu ^ offset);
    }

    start = tsc_low();
    for (pass = 0; pass < passes; ++pass) {
        for (offset = 0; offset < limit; offset += 8) {
            s0 += base[offset + 0];
            s1 += base[offset + 1];
            s2 += base[offset + 2];
            s3 += base[offset + 3];
            s4 += base[offset + 4];
            s5 += base[offset + 5];
            s6 += base[offset + 6];
            s7 += base[offset + 7];
        }
        for (offset = tail; offset < words; ++offset) {
            sum += base[offset];
        }
    }
    end = tsc_low();

    sum += s0 + s1 + s2 + s3 + s4 + s5 + s6 + s7;
    *checksum_out = sum;
    if (end == start) {
        return 0;
    }
    return ((bytes * passes) * 100u) / (end - start);
}

static void serial_write_fixed2(unsigned int x100) {
    serial_write_u32(x100 / 100u);
    serial_write_char('.');
    serial_write_char((char)('0' + ((x100 / 10u) % 10u)));
    serial_write_char((char)('0' + (x100 % 10u)));
}

static void serial_write_hex32(unsigned int value) {
    serial_write_hex16((unsigned short)(value >> 16));
    serial_write_hex16((unsigned short)value);
}

#define MSR_BBL_CR_D0 0x00000088u
#define MSR_BBL_CR_D1 0x00000089u
#define MSR_BBL_CR_D2 0x0000008au
#define MSR_BBL_CR_D3 0x0000008bu
#define MSR_BBL_CR_ADDR 0x00000116u
#define MSR_BBL_CR_CTL 0x00000119u
#define MSR_BBL_CR_TRIG 0x0000011au
#define MSR_BBL_CR_BUSY 0x0000011bu
#define MSR_BBL_CR_CTL3 0x0000011eu

#define BBLCR3_L2_CONFIGURED (1u << 0)
#define BBLCR3_L2_ENABLED (1u << 8)
#define BBLCR3_L2_NOT_PRESENT (1u << 23)
#define BBLCR3_L2_SIZE_MASK (0x1fu << 13)
#define BBLCR3_L2_RANGE_MASK (0x7u << 20)
#define BBLCR3_L2_SIZE_256K (0x01u << 13)
#define BBLCR3_L2_SIZE_512K (0x02u << 13)
#define BBLCR3_L2_SIZE_1M (0x04u << 13)
#define BBLCR3_L2_SIZE_2M (0x08u << 13)
#define BBLCR3_L2_SIZE_4M (0x10u << 13)

#define L2CMD_CR 0x02u
#define L2CMD_CW 0x03u
#define L2CMD_TRR 0x0eu
#define L2CMD_TWW 0x1cu
#define L2CMD_MESI_I 0u

static int signal_l2(unsigned int address, unsigned int data_high,
                     unsigned int data_low, unsigned int way,
                     unsigned int command) {
    unsigned int i;
    unsigned int ctl;

    wrmsr64(MSR_BBL_CR_ADDR, address, 0);
    wrmsr64(MSR_BBL_CR_D0, data_low, data_high);
    wrmsr64(MSR_BBL_CR_D1, data_low, data_high);
    wrmsr64(MSR_BBL_CR_D2, data_low, data_high);
    wrmsr64(MSR_BBL_CR_D3, data_low, data_high);

    ctl = (unsigned int)rdmsr(MSR_BBL_CR_CTL);
    ctl = (ctl & 0xfffffce0u) | command | (way << 8);
    wrmsr64(MSR_BBL_CR_CTL, ctl, 0);
    wrmsr64(MSR_BBL_CR_TRIG, 0, 0);

    for (i = 0; i < 0x100u; ++i) {
        if ((((unsigned int)rdmsr(MSR_BBL_CR_BUSY)) & 1u) == 0) {
            return 0;
        }
    }
    return -1;
}

static int read_l2_reg(unsigned int index) {
    unsigned int value;

    if (signal_l2(index << 5, 0, 0, 0, L2CMD_CR) != 0) {
        return -1;
    }
    value = (unsigned int)rdmsr(MSR_BBL_CR_ADDR);
    return (int)(value >> 21);
}

static int test_l2_address_alias(unsigned int address1, unsigned int address2,
                                 unsigned int data_high, unsigned int data_low) {
    unsigned int d;
    unsigned long long msr;

    if (signal_l2(address1, data_high, data_low, 0, L2CMD_TWW) != 0) {
        return -1;
    }
    if (signal_l2(address2, 0, 0, 0, L2CMD_TRR) != 0) {
        return -1;
    }

    for (d = MSR_BBL_CR_D0; d <= MSR_BBL_CR_D3; ++d) {
        msr = rdmsr(d);
        if ((unsigned int)msr != data_low ||
            (unsigned int)(msr >> 32) != data_high) {
            return (int)((unsigned int)msr & 0xffffu);
        }
    }
    return 0;
}

static int write_l2_reg(unsigned int index, unsigned int data) {
    int v1;
    int v2;
    unsigned int i;

    v1 = read_l2_reg(0);
    if (v1 < 0) {
        return -1;
    }
    v2 = read_l2_reg(2);
    if (v2 < 0) {
        return -1;
    }

    if ((v1 & 0x20) == 0) {
        v2 &= 0x3;
        v2++;
    } else {
        v2 &= 0x7;
    }

    for (i = 0; i < (unsigned int)v2; ++i) {
        unsigned int data1 = (data & 0xffu) << 21;
        unsigned int data2 = (i << 11) & 0x1800u;
        data1 |= data2;
        data2 = (data2 << 6) & 0x20000u;
        data1 |= data2;
        if (signal_l2((index << 5) | data1, 0, 0, 0, L2CMD_CW) != 0) {
            return -1;
        }
    }
    return 0;
}

static void disable_cache(void) {
    unsigned int cr0 = read_cr0();
    cr0 |= 0x40000000u;
    cr0 &= 0xdfffffffu;
    write_cr0(cr0);
}

static void enable_cache(void) {
    unsigned int cr0 = read_cr0();
    cr0 &= 0x9fffffffu;
    write_cr0(cr0);
}

static void wbinvd(void) {
    __asm__ volatile("wbinvd" : : : "memory");
}

static int try_enable_l2(void) {
    unsigned int bbl;
    int l2r0;
    int l2r2;
    int l2r3;
    unsigned int size_bits;
    unsigned int range_bits;
    unsigned int banks;
    unsigned int per_way_bytes;
    unsigned int total_kb;
    unsigned int lines;
    unsigned int line;
    unsigned int way;
    int alias_rc;

    bbl = (unsigned int)rdmsr(MSR_BBL_CR_CTL3);
    if ((bbl & BBLCR3_L2_NOT_PRESENT) != 0) {
        return -1;
    }

    l2r0 = read_l2_reg(0);
    l2r2 = read_l2_reg(2);
    l2r3 = read_l2_reg(3);
    if (l2r0 < 0 || l2r2 < 0 || l2r3 < 0) {
        return -2;
    }

    size_bits = BBLCR3_L2_SIZE_512K;
    range_bits = ((unsigned int)l2r3 & 0x7u) << 20;

    bbl &= ~(BBLCR3_L2_SIZE_MASK | BBLCR3_L2_RANGE_MASK);
    bbl |= size_bits | range_bits;
    wrmsr64(MSR_BBL_CR_CTL3, bbl, 0);

    banks = ((unsigned int)rdmsr(MSR_BBL_CR_CTL3) >> 11) & 0x3u;
    if (banks == 0) {
        banks = 1;
    }
    per_way_bytes = size_bits << 3;
    total_kb = (per_way_bytes * banks * 4u) >> 10;
    serial_write_string("L2 sizefield=");
    serial_write_u32(total_kb);
    serial_write_string("K\r\n");

    alias_rc = test_l2_address_alias(0, per_way_bytes, 0xaaaaaaaau, 0x5555aaaau);
    serial_write_string("L2 alias@size=");
    serial_write_hex16((unsigned short)alias_rc);
    serial_write_string("\r\n");

    enable_cache();

    lines = per_way_bytes / 32u;
    for (line = 0; line < lines; ++line) {
        unsigned int address = (lines - 1u - line) * 32u;
        for (way = 0; way < 4u; ++way) {
            if (signal_l2(address, 0, 0, way, L2CMD_TWW | L2CMD_MESI_I) != 0) {
                return -3;
            }
        }
    }

    disable_cache();
    wbinvd();

    bbl = (unsigned int)rdmsr(MSR_BBL_CR_CTL3);
    bbl |= BBLCR3_L2_CONFIGURED;
    wrmsr64(MSR_BBL_CR_CTL3, bbl, 0);

    wbinvd();
    if (write_l2_reg(5, 0) != 0) {
        return -4;
    }

    bbl = (unsigned int)rdmsr(MSR_BBL_CR_CTL3);
    bbl |= BBLCR3_L2_ENABLED;
    wrmsr64(MSR_BBL_CR_CTL3, bbl, 0);

    enable_cache();
    return 0;
}

static void bandwidth_benchmarks(unsigned int total_bytes) {
    volatile unsigned int* l1 = (volatile unsigned int*)0x00400000u;
    volatile unsigned int* l2 = (volatile unsigned int*)0x00410000u;
    volatile unsigned int* dram = (volatile unsigned int*)0x00800000u;
    volatile unsigned int checksum = 0;
    unsigned int bw;

    if (total_bytes < 0x00c00000u) {
        serial_write_string("Bandwidth: skipped\r\n");
        return;
    }

    serial_write_string("Bandwidth (read, B/cycle):\r\n");
    serial_write_string("bench L1...\r\n");

    bw = bandwidth_read_x100(l1, 16u * 1024u, 2048u, &checksum);
    serial_write_string("L1  16KiB: ");
    serial_write_fixed2(bw);
    serial_write_string(" (");
    serial_write_hex16((unsigned short)checksum);
    serial_write_string(")\r\n");

    serial_write_string("bench L2...\r\n");
    bw = bandwidth_read_x100(l2, 256u * 1024u, 128u, &checksum);
    serial_write_string("L2 256KiB: ");
    serial_write_fixed2(bw);
    serial_write_string(" (");
    serial_write_hex16((unsigned short)checksum);
    serial_write_string(")\r\n");

    serial_write_string("bench DRAM...\r\n");
    bw = bandwidth_read_x100(dram, 4u * 1024u * 1024u, 8u, &checksum);
    serial_write_string("DRAM   4MiB: ");
    serial_write_fixed2(bw);
    serial_write_string(" (");
    serial_write_hex16((unsigned short)checksum);
    serial_write_string(")\r\n");
}

void postcar_resume(unsigned int total_bytes) {
    volatile unsigned int stack_cookie = 0x13579bdfu;
    unsigned int bbl_cr_ctl3 = (unsigned int)rdmsr(MSR_BBL_CR_CTL3);
    int l2r0;
    int l2r2;
    int l2r3;
    int l2rc;

    outb(0x80, POST_DRAM_STACK);
    serial_write_string("post-CAR ok\r\n");
    serial_write_string("DRAM stack @ ");
    serial_write_hex16((unsigned short)(((unsigned int)&stack_cookie) >> 16));
    serial_write_hex16((unsigned short)((unsigned int)&stack_cookie));
    serial_write_string("\r\n");
    serial_write_string("Usable DRAM: ");
    serial_write_u32(total_bytes >> 10);
    serial_write_string("K\r\n");
    serial_write_string("BBL_CR_CTL3=");
    serial_write_hex32(bbl_cr_ctl3);
    serial_write_string("\r\n");
    l2r0 = read_l2_reg(0);
    l2r2 = read_l2_reg(2);
    l2r3 = read_l2_reg(3);
    serial_write_string("L2REG0=");
    serial_write_hex8((unsigned char)l2r0);
    serial_write_string(" L2REG2=");
    serial_write_hex8((unsigned char)l2r2);
    serial_write_string(" L2REG3=");
    serial_write_hex8((unsigned char)l2r3);
    serial_write_string("\r\n");
    serial_write_string("L2 init...");
    l2rc = try_enable_l2();
    serial_write_string(" rc=");
    serial_write_hex8((unsigned char)l2rc);
    serial_write_string("\r\n");
    bbl_cr_ctl3 = (unsigned int)rdmsr(MSR_BBL_CR_CTL3);
    serial_write_string("BBL_CR_CTL3*=");
    serial_write_hex32(bbl_cr_ctl3);
    serial_write_string("\r\n");
    bandwidth_benchmarks(total_bytes);

    for (;;) {
        __asm__ volatile("hlt");
    }
}

void c_entry(void) {
    unsigned int value;
    unsigned int total_bytes = 0;
    unsigned int wb_mask;
    unsigned int new_stack_top;
    unsigned short smba;
    unsigned short pci_cmd;
    unsigned short hostcfg;
    unsigned char slot;
    int probe_rc;
    unsigned char spd[64];
    unsigned char found_spd = 0;

    outb(0x80, POST_ENTER_C);
    serial_write_string("hello from c\r\n");
    serial_write_string("00:07.3=");
    outb(0x80, POST_PCI_PROBE_START);
    outl(0x0cf8, 0x80003b00u);
    value = inl(0x0cfc);
    serial_write_hex16((unsigned short)value);
    serial_write_string("\r\n");

    pci_cmd = pci_read16(0, 7, 3, 0x04);
    if ((pci_cmd & 0x0001u) == 0) {
        pci_write16(0, 7, 3, 0x04, (unsigned short)(pci_cmd | 0x0001u));
    }

    outb(0x80, POST_SMBUS_PROBE_START);
    smba = (unsigned short)(pci_read16(0, 7, 3, 0x90) & 0xfff0u);
    hostcfg = pci_read16(0, 7, 3, 0xd2);

    serial_write_string("SMBBA=");
    serial_write_hex16(smba);
    serial_write_string("\r\n");

    serial_write_string("HOSTCFG=");
    serial_write_hex16(hostcfg);
    serial_write_string("\r\n");

    if (smba == 0) {
        pci_write16(0, 7, 3, 0x90, 0x1000);
        smba = (unsigned short)(pci_read16(0, 7, 3, 0x90) & 0xfff0u);
        serial_write_string("SMBBA*=");
        serial_write_hex16(smba);
        serial_write_string("\r\n");
    }

    if ((hostcfg & 0x0001u) == 0) {
        hostcfg |= 0x0001u;
        pci_write16(0, 7, 3, 0xd2, hostcfg);
        hostcfg = pci_read16(0, 7, 3, 0xd2);
        serial_write_string("HOSTCFG*=");
        serial_write_hex16(hostcfg);
        serial_write_string("\r\n");
    }

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
    serial_write_string("DRAM test...\r\n");
    if (dram_test_rw() != 0) {
        die_with_post(POST_DRAM_TEST_FAIL);
    }
    outb(0x80, POST_DRAM_TEST_PASS);
    serial_write_string("DRAM test ok\r\n");
    outb(0x80, POST_LEAVE_CAR);
    serial_write_string("Leaving CAR...\r\n");
    wb_mask = dram_mtrr_mask(total_bytes);
    new_stack_top = dram_stack_top(total_bytes);
    postcar_transition(new_stack_top, wb_mask, total_bytes);
    for (;;) {
    }
}
