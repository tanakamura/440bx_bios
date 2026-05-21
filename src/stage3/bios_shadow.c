#include "bios_shadow.h"

#include "bios_io.h"
#include "bios_mtrr.h"
#include "bios_pci.h"
#include "bios_serial.h"
#include "shared_service/service_table.h"

#define BIOS_RUNTIME_GDT_LINEAR 0x000ff800u
#define VGA_BIOS_LINEAR SHARED_ROM_LOW_BASE
#define VGA_BIOS_CAPACITY (BIOS_LOAD_LINEAR - VGA_BIOS_LINEAR)

static unsigned char vgabios_shadow_ready;
static unsigned char vgabios_initialized;

static const unsigned long long bios_gdt_template[] = {
    0x0000000000000000ull, 0x00cf9b000000ffffull, 0x00cf93000000ffffull,
    0x00009b0fe000ffffull, 0x0000930fe000ffffull,
};

struct gdtr32 {
    unsigned short limit;
    unsigned int base;
} __attribute__((packed));

static unsigned long long rdmsr64(unsigned int msr) {
    unsigned int lo;
    unsigned int hi;
    __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((unsigned long long)hi << 32) | lo;
}

static void wrmsr64(unsigned int msr, unsigned int lo, unsigned int hi) {
    __asm__ volatile("wrmsr" : : "c"(msr), "a"(lo), "d"(hi));
}

static void cache_writeback_invalidate(void) {
    __asm__ volatile("wbinvd" : : : "memory");
}

static void load_bios_gdt(const unsigned long long* gdt) {
    struct gdtr32 gdtr;
    gdtr.limit = (unsigned short)(sizeof(bios_gdt_template) - 1u);
    gdtr.base = (unsigned int)gdt;

    __asm__ volatile("lgdt %0" : : "m"(gdtr) : "memory");
    __asm__ volatile(
        "movw $0x10, %%ax\n\t"
        "movw %%ax, %%ds\n\t"
        "movw %%ax, %%es\n\t"
        "movw %%ax, %%fs\n\t"
        "movw %%ax, %%gs\n\t"
        "movw %%ax, %%ss\n\t"
        :
        :
        : "eax", "memory");
}

static void cache_disable_for_mtrr_update(void) {
    __asm__ volatile(
        "movl %%cr0, %%eax\n\t"
        "orl $0x40000000, %%eax\n\t"
        "andl $0xdfffffff, %%eax\n\t"
        "movl %%eax, %%cr0\n\t"
        "wbinvd"
        :
        :
        : "eax", "memory");
}

static void cache_enable_after_mtrr_update(void) {
    __asm__ volatile(
        "wbinvd\n\t"
        "movl %%cr0, %%eax\n\t"
        "andl $0x9fffffff, %%eax\n\t"
        "movl %%eax, %%cr0"
        :
        :
        : "eax", "memory");
}

static void enable_shadow_wb_mtrrs(void) {
    unsigned long long def_type = rdmsr64(IA32_MTRR_DEF_TYPE);
    unsigned int def_lo = (unsigned int)def_type;
    unsigned int def_hi = (unsigned int)(def_type >> 32);

    cache_disable_for_mtrr_update();
    wrmsr64(IA32_MTRR_DEF_TYPE, (def_lo & ~MTRR_DEF_TYPE_E), def_hi);
    wrmsr64(IA32_MTRR_FIX4K_C0000, 0x06060606u, 0x06060606u);
    wrmsr64(IA32_MTRR_FIX4K_C8000, 0x06060606u, 0x06060606u);
    wrmsr64(IA32_MTRR_FIX4K_D0000, 0x06060606u, 0x06060606u);
    wrmsr64(IA32_MTRR_FIX4K_D8000, 0x06060606u, 0x06060606u);
    wrmsr64(IA32_MTRR_FIX4K_E0000, 0x06060606u, 0x06060606u);
    wrmsr64(IA32_MTRR_FIX4K_E8000, 0x06060606u, 0x06060606u);
    wrmsr64(IA32_MTRR_FIX4K_F0000, 0x06060606u, 0x06060606u);
    wrmsr64(IA32_MTRR_FIX4K_F8000, 0x06060606u, 0x06060606u);
    wrmsr64(IA32_MTRR_DEF_TYPE, def_lo, def_hi);
    cache_enable_after_mtrr_update();
    serial_write_string("MTRR shadow C-F WB\r\n");
}

static void enable_shadow_dram(void) {
    unsigned char old_pam0;
    unsigned char pam;

    old_pam0 = pci_read8(0, 0, 0, 0x59);

    cache_writeback_invalidate();
    pci_write8(0, 0, 0, 0x59, (unsigned char)(old_pam0 | 0x30u));
    for (pam = 0x5au; pam <= 0x5fu; ++pam) {
        pci_write8(0, 0, 0, pam, 0x33u);
    }
    cache_writeback_invalidate();

    serial_write_string("PAM shadow RAM C-F old=");
    serial_write_hex8(old_pam0);
    serial_write_string(" new=");
    serial_write_hex8(pci_read8(0, 0, 0, 0x59));
    serial_write_string("\r\n");
}

static void clear_shadow_window(void) {
    volatile unsigned int* p = (volatile unsigned int*)VGA_BIOS_LINEAR;
    volatile unsigned int* end = (volatile unsigned int*)0x000e0000u;

    while (p < end) {
        *p++ = 0u;
    }
}

static void install_runtime_gdt(void) {
    volatile unsigned long long* gdt =
        (volatile unsigned long long*)BIOS_RUNTIME_GDT_LINEAR;
    unsigned int i;

    for (i = 0; i < sizeof(bios_gdt_template) / sizeof(bios_gdt_template[0]);
         ++i) {
        gdt[i] = bios_gdt_template[i];
    }
    load_bios_gdt((const unsigned long long*)BIOS_RUNTIME_GDT_LINEAR);
}

void bios_shadow_install(unsigned char already_ready) {
    if (already_ready != 0u) {
        install_runtime_gdt();
        serial_write_string("PAM shadow already ready\r\n");
        return;
    }

    load_bios_gdt(bios_gdt_template);
    enable_shadow_dram();
    clear_shadow_window();
    enable_shadow_wb_mtrrs();
    install_runtime_gdt();
}

void bios_shadow_install_vgabios(unsigned int blob_linear,
                                 blob_load_fn load,
                                 unsigned int total_bytes) {
    struct blob_status status;
    unsigned int load_addr = VGA_BIOS_LINEAR;
    unsigned int size;
    unsigned int i;
    unsigned char sum = 0u;
    int rc;

    vgabios_shadow_ready = 0u;
    if (blob_linear == 0u) {
        return;
    }
    if (load == 0) {
        serial_write_string("VBIOS blob service missing\r\n");
        return;
    }

    serial_write_string("VBIOS @ 000c0000...");
    rc = load(SHARED_PAYLOAD_ID_VGABIOS, (void*)VGA_BIOS_LINEAR,
              VGA_BIOS_CAPACITY, &load_addr, &status, total_bytes);
    serial_write_string("\r\n");
    if (rc != 0) {
        serial_write_string("VBIOS blob failed rc=");
        serial_write_hex8((unsigned char)rc);
        serial_write_string(" block=");
        serial_write_hex32(status.block);
        serial_write_string("\r\n");
        return;
    }

    if (*(volatile unsigned char*)VGA_BIOS_LINEAR != 0x55u ||
        *(volatile unsigned char*)(VGA_BIOS_LINEAR + 1u) != 0xaau) {
        serial_write_string("VBIOS bad signature\r\n");
        return;
    }

    size =
        (unsigned int)(*(volatile unsigned char*)(VGA_BIOS_LINEAR + 2u)) * 512u;
    if (size == 0u || size > VGA_BIOS_CAPACITY) {
        serial_write_string("VBIOS bad size\r\n");
        return;
    }
    for (i = 0u; i < size; ++i) {
        sum = (unsigned char)(sum +
                              *(volatile unsigned char*)(VGA_BIOS_LINEAR + i));
    }
    if (sum != 0u) {
        serial_write_string("VBIOS bad checksum=");
        serial_write_hex8(sum);
        serial_write_string("\r\n");
        return;
    }

    vgabios_shadow_ready = 1u;
    serial_write_string("VBIOS ok size=");
    serial_write_hex8(*(volatile unsigned char*)(VGA_BIOS_LINEAR + 2u));
    serial_write_string("*512\r\n");
}

void bios_shadow_init_vgabios(bios_shadow_void_fn init_pm32) {
    if (vgabios_shadow_ready == 0u || vgabios_initialized != 0u ||
        init_pm32 == 0) {
        return;
    }
    serial_write_string("VBIOS init C000:0003...\r\n");
    cache_writeback_invalidate();
    init_pm32();
    cache_writeback_invalidate();
    vgabios_initialized = 1u;
    serial_write_string("VBIOS init returned\r\n");
}
