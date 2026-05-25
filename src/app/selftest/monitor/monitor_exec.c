#include "app_boot_abi.h"
#include "bios_io.h"

#define MON_INIT 127u

#define MON_READ8 2u
#define MON_READ16 3u
#define MON_READ32 4u

#define MON_WRITE8 5u
#define MON_WRITE16 6u
#define MON_WRITE32 7u

#define MON_IN8 8u
#define MON_IN16 9u
#define MON_IN32 10u

#define MON_OUT8 11u
#define MON_OUT16 12u
#define MON_OUT32 13u

#define MON_RDMSR 14u
#define MON_WRMSR 15u

#define MON_LOADBIN 16u
#define MON_RUNBIN 17u

#define MON_LOADBIN16 18u
#define MON_RUNBIN16 19u
#define MON_WBINVD 20u

#define MON_ACK 0xfeu
#define MON_EOF 0xffu

#define UART_BASE 0x03f8u
#define UART_DATA UART_BASE
#define UART_LSR (UART_BASE + 5u)

#define LOADBIN32_BASE 0x01000000u
#define LOADBIN16_BASE 0x00010000u

static unsigned char uart_get8(void) {
    while ((inb(UART_LSR) & 0x01u) == 0u) {
    }
    return inb(UART_DATA);
}

static unsigned short uart_get16(void) {
    unsigned short v = uart_get8();
    v |= (unsigned short)uart_get8() << 8;
    return v;
}

static unsigned int uart_get32(void) {
    unsigned int v = uart_get16();
    v |= (unsigned int)uart_get16() << 16;
    return v;
}

static void uart_put8(unsigned char v) {
    while ((inb(UART_LSR) & 0x20u) == 0u) {
    }
    outb(UART_DATA, v);
}

static void uart_put16(unsigned short v) {
    uart_put8((unsigned char)v);
    uart_put8((unsigned char)(v >> 8));
}

static void uart_put32(unsigned int v) {
    uart_put16((unsigned short)v);
    uart_put16((unsigned short)(v >> 16));
}

static void mon_read8(void) {
    unsigned int addr = uart_get32();
    uart_put8(*(volatile unsigned char*)addr);
}

static void mon_read16(void) {
    unsigned int addr = uart_get32();
    uart_put16(*(volatile unsigned short*)addr);
}

static void mon_read32(void) {
    unsigned int addr = uart_get32();
    uart_put32(*(volatile unsigned int*)addr);
}

static void mon_write8(void) {
    unsigned int addr = uart_get32();
    unsigned char val = uart_get8();
    *(volatile unsigned char*)addr = val;
    uart_put8(MON_ACK);
}

static void mon_write16(void) {
    unsigned int addr = uart_get32();
    unsigned short val = uart_get16();
    *(volatile unsigned short*)addr = val;
    uart_put8(MON_ACK);
}

static void mon_write32(void) {
    unsigned int addr = uart_get32();
    unsigned int val = uart_get32();
    *(volatile unsigned int*)addr = val;
    uart_put8(MON_ACK);
}

static void mon_in8(void) {
    uart_put8(inb(uart_get16()));
}

static void mon_in16(void) {
    uart_put16(inw(uart_get16()));
}

static void mon_in32(void) {
    uart_put32(inl(uart_get16()));
}

static void mon_out8(void) {
    unsigned short port = uart_get16();
    unsigned char val = uart_get8();
    outb(port, val);
    uart_put8(MON_ACK);
}

static void mon_out16(void) {
    unsigned short port = uart_get16();
    unsigned short val = uart_get16();
    outw(port, val);
    uart_put8(MON_ACK);
}

static void mon_out32(void) {
    unsigned short port = uart_get16();
    unsigned int val = uart_get32();
    outl(port, val);
    uart_put8(MON_ACK);
}

static void mon_rdmsr(void) {
    unsigned int msr = uart_get32();
    unsigned int lo;
    unsigned int hi;

    __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    uart_put32(lo);
    uart_put32(hi);
}

static void mon_wrmsr(void) {
    unsigned int msr = uart_get32();
    unsigned int lo = uart_get32();
    unsigned int hi = uart_get32();

    __asm__ volatile("wrmsr" : : "c"(msr), "a"(lo), "d"(hi));
    uart_put8(MON_ACK);
}

static void mon_loadbin(unsigned int base) {
    unsigned int len = uart_get32();
    unsigned char checksum = uart_get8();
    unsigned char sum = 0u;
    unsigned int i;

    for (i = 0u; i < len; ++i) {
        unsigned char c = uart_get8();
        *(volatile unsigned char*)(base + i) = c;
    }
    for (i = 0u; i < len; ++i) {
        sum ^= *(volatile unsigned char*)(base + i);
    }
    uart_put8(sum == checksum ? MON_ACK : MON_EOF);
}

static void mon_runbin32(void) {
    unsigned int (*entry)(void) = (unsigned int (*)(void))LOADBIN32_BASE;
    unsigned int rc = entry();

    uart_put8(MON_EOF);
    uart_put32(rc);
}

static void mon_wbinvd(void) {
    __asm__ volatile("wbinvd" : : : "memory");
    uart_put8(MON_ACK);
}

static void mon_unsupported(void) { uart_put8(MON_EOF); }

__attribute__((section(".text.entry"), used)) int app_entry(
    const struct app_boot_context* ctx) {
    (void)ctx;

    for (;;) {
        switch (uart_get8()) {
            case MON_INIT:
                uart_put8(1u);
                break;
            case MON_READ8:
                mon_read8();
                break;
            case MON_READ16:
                mon_read16();
                break;
            case MON_READ32:
                mon_read32();
                break;
            case MON_WRITE8:
                mon_write8();
                break;
            case MON_WRITE16:
                mon_write16();
                break;
            case MON_WRITE32:
                mon_write32();
                break;
            case MON_IN8:
                mon_in8();
                break;
            case MON_IN16:
                mon_in16();
                break;
            case MON_IN32:
                mon_in32();
                break;
            case MON_OUT8:
                mon_out8();
                break;
            case MON_OUT16:
                mon_out16();
                break;
            case MON_OUT32:
                mon_out32();
                break;
            case MON_RDMSR:
                mon_rdmsr();
                break;
            case MON_WRMSR:
                mon_wrmsr();
                break;
            case MON_LOADBIN:
                mon_loadbin(LOADBIN32_BASE);
                break;
            case MON_RUNBIN:
                mon_runbin32();
                break;
            case MON_LOADBIN16:
                mon_loadbin(LOADBIN16_BASE);
                break;
            case MON_WBINVD:
                mon_wbinvd();
                break;
            case MON_RUNBIN16:
            default:
                mon_unsupported();
                break;
        }
    }
}
