#include "app/legacy/legacy_timer.h"

#include "app/legacy/legacy_io.h"

#define IA32_APIC_BASE 0x0000001bu
#define APIC_BASE_ENABLE 0x00000800u
#define BDA_TICK_COUNT 0x046cu
#define BDA_MIDNIGHT_FLAG 0x0470u

static unsigned int tick_counter = 0;
static unsigned char tick_initialized = 0;
static unsigned short tick_last_raw = 0;
static unsigned int tick_subcount = 0;
static unsigned char timer_irq_enabled = 0;

static unsigned long long rdmsr64(unsigned int msr) {
    unsigned int lo;
    unsigned int hi;
    __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((unsigned long long)hi << 32) | lo;
}

static void wrmsr64(unsigned int msr, unsigned int lo, unsigned int hi) {
    __asm__ volatile("wrmsr" : : "c"(msr), "a"(lo), "d"(hi) : "memory");
}

static unsigned short pit_read_counter0(void) {
    unsigned char lo;
    unsigned char hi;
    outb(0x0043u, 0x00u);
    lo = inb(0x0040u);
    hi = inb(0x0040u);
    return (unsigned short)(((unsigned short)hi << 8) | lo);
}

static void set_tick_counter(unsigned int ticks) {
    tick_counter = ticks;
    *(volatile unsigned int*)BDA_TICK_COUNT = ticks;
}

static void io_wait(void) { outb(0x0080u, 0x00u); }

static void disable_local_apic(void) {
    unsigned long long apic_base = rdmsr64(IA32_APIC_BASE);
    if (((unsigned int)apic_base & APIC_BASE_ENABLE) != 0u) {
        wrmsr64(IA32_APIC_BASE, ((unsigned int)apic_base & ~APIC_BASE_ENABLE),
                (unsigned int)(apic_base >> 32));
    }
}

static void init_pit(void) {
    outb(0x0043u, 0x36u);
    outb(0x0040u, 0x00u);
    outb(0x0040u, 0x00u);
    tick_initialized = 0;
    tick_subcount = 0;
    set_tick_counter(0u);
    *(volatile unsigned char*)BDA_MIDNIGHT_FLAG = 0u;
    timer_irq_enabled = 0u;
}

static void init_pic_for_timer(void) {
    disable_local_apic();
    outb(0x0022u, 0x70u);
    io_wait();
    outb(0x0023u, 0x00u);
    io_wait();
    outb(0x0020u, 0x11u);
    io_wait();
    outb(0x00a0u, 0x11u);
    io_wait();
    outb(0x0021u, 0x08u);
    io_wait();
    outb(0x00a1u, 0x70u);
    io_wait();
    outb(0x0021u, 0x04u);
    io_wait();
    outb(0x00a1u, 0x02u);
    io_wait();
    outb(0x0021u, 0x01u);
    io_wait();
    outb(0x00a1u, 0x01u);
    io_wait();
    outb(0x0020u, 0x20u);
    outb(0x00a0u, 0x20u);
    io_wait();
    outb(0x0021u, 0xfeu);
    outb(0x00a1u, 0xffu);
    timer_irq_enabled = 1u;
}

void legacy_timer_init(void) {
    init_pit();
    init_pic_for_timer();
}

void legacy_timer_update(void) {
    unsigned short raw;
    if (timer_irq_enabled) {
        tick_counter = *(volatile unsigned int*)BDA_TICK_COUNT;
        return;
    }

    raw = pit_read_counter0();
    if (!tick_initialized) {
        tick_initialized = 1;
        tick_last_raw = raw;
        *(volatile unsigned int*)BDA_TICK_COUNT = tick_counter;
        return;
    }

    tick_subcount += (unsigned short)((tick_last_raw - raw) & 0xffffu);
    tick_last_raw = raw;

    while (tick_subcount >= 65536u) {
        tick_subcount -= 65536u;
        ++tick_counter;
        if (tick_counter >= 0x001800b0u) {
            tick_counter = 0;
            *(volatile unsigned char*)BDA_MIDNIGHT_FLAG = 1u;
        }
    }
    *(volatile unsigned int*)BDA_TICK_COUNT = tick_counter;
}

unsigned int* legacy_timer_tick_counter(void) { return &tick_counter; }
