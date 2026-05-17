#include "blob.h"

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

__attribute__((section(".stage2.entry"), used)) void qemu_stage2_entry(
    unsigned int total_bytes, unsigned int aux_linear) {
    typedef void (*bios_entry_fn)(unsigned int, unsigned int);
    volatile unsigned int* aux = (volatile unsigned int*)aux_linear;
    const void* stage3_blob = (const void*)aux[BOOT_AUX_STAGE3_BLOB];
    blob_expand_fn expand = (blob_expand_fn)BLOB_SERVICE_LINEAR;
    struct blob_status status;
    int rc;

    zero_bss();
    serial_write_string("qemu stage2 @ 00080000\r\n");
    aux[BOOT_AUX_SHADOW_READY] = 1u;

    rc = expand(stage3_blob, (void*)BLOB_STAGE_LINEAR, (void*)BIOS_LOAD_LINEAR,
                BIOS_LOAD_CAPACITY, &status);
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
    ((bios_entry_fn)BIOS32_QEMU_ENTRY)(total_bytes, aux_linear);
    for (;;) {
        __asm__ volatile("hlt");
    }
}
