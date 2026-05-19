#include "bios_memtest.h"

#include "bios_io.h"
#include "bios_memory.h"
#include "bios_serial.h"
#include "post_code.h"
#include "shared_service/service_table.h"

#define BIOS_MTRR_SAVE_MAX 8u
#define BIOS_MEMTEST_START 0x00100000u
#define BIOS_MEMTEST_MARK_STEP 0x00100000u
#define IA32_MTRRCAP 0x0feu
#define IA32_MTRR_PHYSBASE0 0x200u
#define IA32_MTRR_PHYSMASK0 0x201u
#define IA32_MTRR_DEF_TYPE 0x2ffu
#define MTRR_DEF_TYPE_TYPE_MASK 0x000000ffu
#define MTRR_DEF_TYPE_E 0x00000800u
#define MTRR_PHYSMASK_VALID 0x00000800u

struct bios_mtrr_saved_state {
    unsigned char count;
    unsigned long long def_type;
    unsigned long long base[BIOS_MTRR_SAVE_MAX];
    unsigned long long mask[BIOS_MTRR_SAVE_MAX];
};

static unsigned long long rdmsr64(unsigned int msr) {
    unsigned int lo;
    unsigned int hi;
    __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((unsigned long long)hi << 32) | lo;
}

static void wrmsr64(unsigned int msr, unsigned int lo, unsigned int hi) {
    __asm__ volatile("wrmsr" : : "c"(msr), "a"(lo), "d"(hi));
}

static void cache_disable_for_mtrr_update(void) {
    unsigned int eax;

    __asm__ volatile("cli" : : : "memory");
    __asm__ volatile("mov %%cr0, %0" : "=r"(eax));
    eax = (eax | 0x40000000u) & ~0x20000000u;
    __asm__ volatile("mov %0, %%cr0" : : "r"(eax) : "memory");
    __asm__ volatile("wbinvd" : : : "memory");
    __asm__ volatile("mov %%cr4, %0" : "=r"(eax));
    eax &= ~0x80u;
    __asm__ volatile("mov %0, %%cr4" : : "r"(eax) : "memory");
}

static void cache_enable_after_mtrr_update(void) {
    unsigned int eax;

    __asm__ volatile("wbinvd" : : : "memory");
    __asm__ volatile("mov %%cr0, %0" : "=r"(eax));
    eax &= ~0x60000000u;
    __asm__ volatile("mov %0, %%cr0" : : "r"(eax) : "memory");
}

static unsigned char mtrr_variable_count(void) {
    unsigned int count = (unsigned int)(rdmsr64(IA32_MTRRCAP) & 0xffu);
    if (count > BIOS_MTRR_SAVE_MAX) {
        count = BIOS_MTRR_SAVE_MAX;
    }
    return (unsigned char)count;
}

static void mtrr_save_and_uc_1m_plus(struct bios_mtrr_saved_state* saved) {
    unsigned int i;
    unsigned int def_lo;
    unsigned int def_hi;

    saved->count = mtrr_variable_count();
    saved->def_type = rdmsr64(IA32_MTRR_DEF_TYPE);
    for (i = 0u; i < saved->count; ++i) {
        saved->base[i] = rdmsr64(IA32_MTRR_PHYSBASE0 + i * 2u);
        saved->mask[i] = rdmsr64(IA32_MTRR_PHYSMASK0 + i * 2u);
    }

    def_lo = (unsigned int)saved->def_type;
    def_hi = (unsigned int)(saved->def_type >> 32);
    cache_disable_for_mtrr_update();
    wrmsr64(IA32_MTRR_DEF_TYPE, def_lo & ~MTRR_DEF_TYPE_E, def_hi);
    for (i = 0u; i < saved->count; ++i) {
        unsigned int mask_lo =
            (unsigned int)saved->mask[i] & ~MTRR_PHYSMASK_VALID;
        unsigned int mask_hi = (unsigned int)(saved->mask[i] >> 32);
        wrmsr64(IA32_MTRR_PHYSMASK0 + i * 2u, mask_lo, mask_hi);
    }
    wrmsr64(IA32_MTRR_DEF_TYPE, def_lo & ~MTRR_DEF_TYPE_TYPE_MASK, def_hi);
    cache_enable_after_mtrr_update();
}

static void mtrr_restore_saved(const struct bios_mtrr_saved_state* saved) {
    unsigned int i;
    unsigned int def_lo = (unsigned int)saved->def_type;
    unsigned int def_hi = (unsigned int)(saved->def_type >> 32);

    cache_disable_for_mtrr_update();
    wrmsr64(IA32_MTRR_DEF_TYPE, def_lo & ~MTRR_DEF_TYPE_E, def_hi);
    for (i = 0u; i < saved->count; ++i) {
        wrmsr64(IA32_MTRR_PHYSBASE0 + i * 2u,
                (unsigned int)saved->base[i],
                (unsigned int)(saved->base[i] >> 32));
        wrmsr64(IA32_MTRR_PHYSMASK0 + i * 2u,
                (unsigned int)saved->mask[i],
                (unsigned int)(saved->mask[i] >> 32));
    }
    wrmsr64(IA32_MTRR_DEF_TYPE, def_lo, def_hi);
    cache_enable_after_mtrr_update();
}

static void memtest_print_kib_ok(unsigned int bytes) {
    unsigned int kib = bytes >> 10;
    unsigned int divisor = 1000000u;

    while (divisor != 0u) {
        serial_write_char((char)('0' + ((kib / divisor) % 10u)));
        divisor /= 10u;
    }
    serial_write_string(" KiB OK");
}

static void memtest_progress(unsigned int addr, unsigned int* next_mark) {
    while (addr >= *next_mark) {
        serial_write_char('\r');
        memtest_print_kib_ok(*next_mark);
        *next_mark += BIOS_MEMTEST_MARK_STEP;
    }
}

static unsigned int memtest_skip_end(
    const struct shared_service_table* service, unsigned int addr) {
    if (service != 0 && service->service_base != 0u &&
        addr >= service->service_base &&
        addr < service->service_base + service->service_size) {
        return service->service_base + service->service_size;
    }
    if (service != 0 && service->blob_stage != 0u &&
        service->blob_stage_size != 0u && addr >= service->blob_stage &&
        addr < service->blob_stage + service->blob_stage_size) {
        return service->blob_stage + service->blob_stage_size;
    }
    return addr;
}

static int memtest_range_uncached(const struct shared_service_table* service,
                                  unsigned int end) {
    unsigned int addr;
    unsigned int next_mark = BIOS_MEMTEST_START + BIOS_MEMTEST_MARK_STEP;

    if (end <= BIOS_MEMTEST_START) {
        return 0;
    }

    memtest_print_kib_ok(BIOS_MEMTEST_START);
    for (addr = BIOS_MEMTEST_START; addr + 4u <= end;) {
        unsigned int skip_end = memtest_skip_end(service, addr);
        if (skip_end != addr) {
            addr = skip_end;
            memtest_progress(addr, &next_mark);
            continue;
        }
        *(volatile unsigned int*)addr = addr ^ 0xa5a55a5au;
        addr += 4u;
        memtest_progress(addr, &next_mark);
    }
    for (addr = BIOS_MEMTEST_START; addr + 4u <= end;) {
        unsigned int expected;
        unsigned int got;
        unsigned int skip_end = memtest_skip_end(service, addr);
        if (skip_end != addr) {
            addr = skip_end;
            continue;
        }
        expected = addr ^ 0xa5a55a5au;
        got = *(volatile unsigned int*)addr;
        if (got != expected) {
            serial_write_string("\r\nMemTest fail @ ");
            serial_write_hex32(addr);
            serial_write_string(" got=");
            serial_write_hex32(got);
            serial_write_string(" exp=");
            serial_write_hex32(expected);
            serial_write_string("\r\n");
            return -1;
        }
        *(volatile unsigned int*)addr = ~expected;
        addr += 4u;
    }
    serial_write_string("\r\n");
    return 0;
}

void bios_memtest_run_optional(unsigned char enable, unsigned int total_bytes,
                               const struct shared_service_table* service) {
    unsigned int end;
    int rc;
    struct bios_mtrr_saved_state saved;

    if (enable == 0u) {
        return;
    }

    end = bios_memory_extended_usable_end(total_bytes);
    serial_write_string("Memtest UC ");
    serial_write_hex32(BIOS_MEMTEST_START);
    serial_write_string("-");
    serial_write_hex32(end);
    serial_write_string("\r\n");

    mtrr_save_and_uc_1m_plus(&saved);
    rc = memtest_range_uncached(service, end);
    mtrr_restore_saved(&saved);

    if (rc != 0) {
        outb(0x80, POST_DRAM_TEST_FAIL);
        for (;;) {
            __asm__ volatile("hlt");
        }
    }
    serial_write_string("Memtest ok\r\n");
}
