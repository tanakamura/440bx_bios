#include "selftest_abi.h"

static inline void outb(unsigned short port, unsigned char value) {
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline unsigned char inb(unsigned short port) {
    unsigned char value;
    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline unsigned short inw(unsigned short port) {
    unsigned short value;
    __asm__ volatile("inw %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline unsigned int inl(unsigned short port) {
    unsigned int value;
    __asm__ volatile("inl %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

extern unsigned int s3test_run_uacpi(unsigned int rsdp);

static void serial_write_char(char ch) {
    while ((inb(0x03f8u + 5u) & 0x20u) == 0) {
    }
    outb(0x03f8u, (unsigned char)ch);
}

static void serial_write_string(const char* s) {
    while (*s != '\0') {
        serial_write_char(*s++);
    }
}

static void serial_write_hex4(unsigned char value) {
    value &= 0x0fu;
    if (value < 10u) {
        serial_write_char((char)('0' + value));
    } else {
        serial_write_char((char)('a' + (value - 10u)));
    }
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

static unsigned int read32(const void* ptr) {
    const unsigned char* p = (const unsigned char*)ptr;
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8) |
           ((unsigned int)p[2] << 16) | ((unsigned int)p[3] << 24);
}

static int memeq(const unsigned char* p, const char* s, unsigned int len) {
    unsigned int i;
    for (i = 0u; i < len; ++i) {
        if (p[i] != (unsigned char)s[i]) {
            return 0;
        }
    }
    return 1;
}

static void dump_pm(unsigned int pm1_evt, unsigned int pm1_cnt) {
    serial_write_string("PM1_STS=");
    serial_write_hex16(inw((unsigned short)pm1_evt));
    serial_write_string(" PM1_EN=");
    serial_write_hex16(inw((unsigned short)(pm1_evt + 2u)));
    serial_write_string(" PM1_CNT=");
    serial_write_hex16(inw((unsigned short)pm1_cnt));
    serial_write_string(" PM_TMR=");
    serial_write_hex32(inl((unsigned short)(pm1_evt + 8u)));
    serial_write_string(" GPE=");
    serial_write_hex8(inb((unsigned short)(pm1_evt + 0x0cu)));
    serial_write_char('/');
    serial_write_hex8(inb((unsigned short)(pm1_evt + 0x0eu)));
    serial_write_string("\r\n");
}

unsigned int s3test_entry(const struct selftest_runtime_info* info) {
    unsigned int boot_params = info->boot_params;
    unsigned int rsdp = info->platform.acpi_rsdp_linear;
    unsigned int pm1_evt = info->platform.acpi_pm1_evt;
    unsigned int pm1_cnt = info->platform.acpi_pm1_cnt;
    const unsigned char* rsdp_ptr = (const unsigned char*)rsdp;
    const unsigned char* bp = (const unsigned char*)boot_params;
    unsigned int ok = 1u;

    serial_write_string("S3TEST START\r\n");
    serial_write_string("args info=");
    serial_write_hex32((unsigned int)info);
    serial_write_string(" bp=");
    serial_write_hex32(boot_params);
    serial_write_string(" rsdp=");
    serial_write_hex32(rsdp);
    serial_write_string(" pm1=");
    serial_write_hex32(pm1_evt);
    serial_write_char('/');
    serial_write_hex32(pm1_cnt);
    serial_write_string("\r\n");

    if (!memeq(rsdp_ptr, "RSD PTR ", 8u)) {
        serial_write_string("S3TEST RSDP NG\r\n");
        ok = 0u;
    } else {
        serial_write_string("S3TEST RSDP OK rsdt=");
        serial_write_hex32(read32(rsdp_ptr + 16u));
        serial_write_string("\r\n");
    }

    if (read32(bp + 0x202u) != 0x53726448u) {
        serial_write_string("S3TEST BOOTPARAM NG\r\n");
        ok = 0u;
    } else {
        serial_write_string("S3TEST BOOTPARAM OK e820=");
        serial_write_hex8(bp[0x1e8u]);
        serial_write_string("\r\n");
    }

    dump_pm(pm1_evt, pm1_cnt);
    if (ok != 0u) {
        unsigned int uacpi_rc = s3test_run_uacpi(rsdp);
        if (uacpi_rc != 0u) {
            serial_write_string("S3TEST uACPI NG rc=");
            serial_write_hex32(uacpi_rc);
            serial_write_string("\r\n");
            ok = 0u;
        }
    }
    serial_write_string(ok != 0u ? "S3TEST OK\r\n" : "S3TEST NG\r\n");
    return ok != 0u ? 0x53334f4bu : 0x53334e47u;
}
