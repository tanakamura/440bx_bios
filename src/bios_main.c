#include "acpi_tables.h"
#include "bios_io.h"
#include "bios_maintenance.h"
#include "bios_memtest.h"
#include "bios_memory.h"
#include "bios_nvram.h"
#include "bios_pci.h"
#include "bios_rtc.h"
#include "bios_serial.h"
#include "bios_storage.h"
#include "blob.h"
#include "shared_service/service_table.h"
#include "app/linux_loader/linux_loader.h"
#include "app/legacy/legacy_boot.h"
#include "app/legacy/legacy_floppy.h"
#include "app/legacy/legacy_platform.h"
#include "app/legacy/legacy_runtime.h"
#include "app/legacy/legacy_thunk.h"
#include "post_code.h"

void bios32_entry_c(unsigned int total_bytes, unsigned int aux_blob_linear);
extern void bios_boot_freedos_pm32(void);
extern void bios_call_vgabios_init_pm32(void);
extern unsigned int bios_call_vbe_mode_info_pm32(unsigned int mode);
extern unsigned int bios_call_vbe_set_mode_pm32(unsigned int mode);
extern unsigned char __bss_start[];
extern unsigned char __bss_end[];

static unsigned int bios_total_bytes_global = 0;
static unsigned int bios_vgabios_blob_linear_global = 0;
static unsigned int bios_test_elf_blob_linear_global = 0;
static unsigned int bios_rsdp_linear_global = 0;
static unsigned int bios_acpi_pm1_evt_global = 0;
static unsigned int bios_acpi_pm1_cnt_global = 0;
static unsigned int bios_acpi_gpe0_global = 0;
static unsigned int bios_acpi_gpe0_len_global = 0;
static unsigned int bios_acpi_flags_global = 0;
static struct shared_service_table* bios_shared_service_global = 0;
static unsigned char bios_vgabios_shadow_ready = 0;
static unsigned char bios_vgabios_initialized = 0;
static unsigned char bios_maintenance_requested = 0;
static unsigned char bios_shadow_ready = 0;
static unsigned char bios_nvram_flags0 = BIOS_NVRAM_FLAGS0_DEFAULT;
static unsigned char bios_boot_priority = BIOS_NVRAM_BOOT_PRIORITY_DEFAULT;
static unsigned char bios_linux_vmlinux_partition = 0;
static unsigned char bios_enable_memtest = 0;
static unsigned char bios_run_test_blob = 0;
static char bios_linux_cmdline_suffix[BIOS_NVRAM_CMDLINE_MAX];
static const unsigned int bios_runtime_gdt_linear = 0x000ff800u;
static const unsigned short bios_ebda_segment = 0x0000u;
static const unsigned short bios_dos_base_mem_kb = 640u;
static unsigned char bios_boot_drive = 0x80u;

static unsigned int tsc_low(void);
static void acpi_install_for_linux(void);

static void zero_bss(void) {
    unsigned char* p = __bss_start;
    while (p < __bss_end) {
        *p++ = 0u;
    }
}

static unsigned long long rdmsr64(unsigned int msr) {
    unsigned int lo;
    unsigned int hi;
    __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((unsigned long long)hi << 32) | lo;
}

static void wrmsr64(unsigned int msr, unsigned int lo, unsigned int hi) {
    __asm__ volatile("wrmsr" : : "c"(msr), "a"(lo), "d"(hi));
}

#define IA32_MTRR_FIX4K_C0000 0x268u
#define IA32_MTRR_FIX4K_C8000 0x269u
#define IA32_MTRR_FIX4K_D0000 0x26au
#define IA32_MTRR_FIX4K_D8000 0x26bu
#define IA32_MTRR_FIX4K_E0000 0x26cu
#define IA32_MTRR_FIX4K_E8000 0x26du
#define IA32_MTRR_FIX4K_F0000 0x26eu
#define IA32_MTRR_FIX4K_F8000 0x26fu
#define IA32_MTRR_DEF_TYPE 0x2ffu
#define MTRR_DEF_TYPE_E 0x00000800u

static int nvram_enable_extended_cmos(void) {
    return bios_nvram_enable_extended_cmos();
}

static void nvram_init_defaults(void) {
    bios_nvram_init_defaults();
}

static void nvram_load_settings(void) {
    unsigned int i;
    struct bios_nvram_settings settings;

    bios_nvram_load_settings(&settings);
    bios_nvram_flags0 = settings.flags0;
    bios_boot_priority = settings.boot_priority;
    bios_linux_vmlinux_partition = settings.vmlinux_partition;
    bios_enable_memtest = settings.enable_memtest;
    bios_run_test_blob = settings.run_test_blob;
    for (i = 0; i < BIOS_NVRAM_CMDLINE_MAX; ++i) {
        bios_linux_cmdline_suffix[i] = settings.linux_cmdline_suffix[i];
        if (settings.linux_cmdline_suffix[i] == '\0') {
            break;
        }
    }
    bios_linux_cmdline_suffix[BIOS_NVRAM_CMDLINE_MAX - 1u] = '\0';
}

static void nvram_save_partition(unsigned char part) {
    bios_nvram_save_partition(part);
}

static void nvram_save_flags0(unsigned char flags0) {
    bios_nvram_save_flags0(flags0);
}

static void nvram_consume_test_blob_request(void) {
    if ((bios_nvram_flags0 & BIOS_NVRAM_FLAGS0_RUN_TEST_BLOB) == 0u) {
        return;
    }
    bios_nvram_flags0 =
        (unsigned char)(bios_nvram_flags0 & ~BIOS_NVRAM_FLAGS0_RUN_TEST_BLOB);
    bios_run_test_blob = 0u;
    nvram_save_flags0(bios_nvram_flags0);
    serial_write_string("Test blob request consumed\r\n");
}

static void nvram_save_boot_priority(unsigned char priority) {
    bios_nvram_save_boot_priority(priority);
}

static void nvram_save_cmdline_suffix(const char* text) {
    bios_nvram_save_cmdline_suffix(text);
}

static void nvram_reset_defaults(void) {
    if (nvram_enable_extended_cmos() == 0) {
        nvram_init_defaults();
    }
}

static void maintenance_prompt(void) {
    struct bios_maintenance_config config;

    config.flags0 = &bios_nvram_flags0;
    config.boot_priority = &bios_boot_priority;
    config.vmlinux_partition = &bios_linux_vmlinux_partition;
    config.enable_memtest = &bios_enable_memtest;
    config.run_test_blob = &bios_run_test_blob;
    config.linux_cmdline_suffix = bios_linux_cmdline_suffix;
    config.save_partition = nvram_save_partition;
    config.save_flags0 = nvram_save_flags0;
    config.save_boot_priority = nvram_save_boot_priority;
    config.save_cmdline_suffix = nvram_save_cmdline_suffix;
    config.reset_defaults = nvram_reset_defaults;
    bios_maintenance_prompt(&config);
}

static const unsigned long long bios_gdt_template[] = {
    0x0000000000000000ull, 0x00cf9b000000ffffull, 0x00cf93000000ffffull,
    0x00009b0fe000ffffull, 0x0000930fe000ffffull,
};

struct gdtr32 {
    unsigned short limit;
    unsigned int base;
} __attribute__((packed));

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

static void cache_writeback_invalidate(void) {
    __asm__ volatile("wbinvd" : : : "memory");
}

static void cpu_serialize(void) {
    __asm__ volatile("xorl %%eax, %%eax\n\tcpuid"
                     :
                     :
                     : "eax", "ebx", "ecx", "edx", "memory");
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
    volatile unsigned int* p = (volatile unsigned int*)0x000c0000u;
    volatile unsigned int* end = (volatile unsigned int*)0x000e0000u;

    while (p < end) {
        *p++ = 0u;
    }
}

static void install_runtime_gdt(void) {
    volatile unsigned long long* gdt =
        (volatile unsigned long long*)bios_runtime_gdt_linear;
    unsigned int i;

    for (i = 0; i < sizeof(bios_gdt_template) / sizeof(bios_gdt_template[0]);
         ++i) {
        gdt[i] = bios_gdt_template[i];
    }
    load_bios_gdt((const unsigned long long*)bios_runtime_gdt_linear);
}

#define VGA_BIOS_LINEAR 0x000c0000u
#define VGA_BIOS_CAPACITY (BIOS_LOAD_LINEAR - VGA_BIOS_LINEAR)

static void install_bios_shadow(void) {
    if (bios_shadow_ready != 0u) {
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

static blob_expand_fn bios_blob_expand_fn(void) {
    if (bios_shared_service_global != 0 &&
        bios_shared_service_global->blob_expand != 0u) {
        return (blob_expand_fn)bios_shared_service_global->blob_expand;
    }
    return 0;
}

static void* bios_blob_stage_ptr(void) {
    if (bios_shared_service_global != 0 &&
        bios_shared_service_global->blob_stage != 0u &&
        bios_shared_service_global->blob_stage_size >= BLOB_STAGE_CAPACITY) {
        return (void*)bios_shared_service_global->blob_stage;
    }
    return 0;
}

static void install_vgabios_shadow(void) {
    blob_expand_fn expand = bios_blob_expand_fn();
    void* blob_stage = bios_blob_stage_ptr();
    struct blob_status status;
    unsigned int size;
    unsigned int i;
    unsigned char sum = 0u;
    int rc;

    bios_vgabios_shadow_ready = 0u;
    if (bios_vgabios_blob_linear_global == 0u) {
        return;
    }
    if (expand == 0 || blob_stage == 0) {
        serial_write_string("VBIOS blob service missing\r\n");
        return;
    }

    serial_write_string("VBIOS @ 000c0000...");
    rc = expand((const void*)bios_vgabios_blob_linear_global,
                blob_stage, (void*)VGA_BIOS_LINEAR, VGA_BIOS_CAPACITY,
                &status, bios_total_bytes_global);
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

    bios_vgabios_shadow_ready = 1u;
    serial_write_string("VBIOS ok size=");
    serial_write_hex8(*(volatile unsigned char*)(VGA_BIOS_LINEAR + 2u));
    serial_write_string("*512\r\n");
}

static void init_vgabios_for_linux(void) {
    if (bios_vgabios_shadow_ready == 0u || bios_vgabios_initialized != 0u) {
        return;
    }
    serial_write_string("VBIOS init C000:0003...\r\n");
    cache_writeback_invalidate();
    bios_call_vgabios_init_pm32();
    cache_writeback_invalidate();
    bios_vgabios_initialized = 1u;
    serial_write_string("VBIOS init returned\r\n");
}

static void nvram_record_boot_success(unsigned char kind);

static void legacy_hdd_get_geometry_cb(struct legacy_hdd_geometry* geometry) {
    struct bios_hdd_geometry bios_geometry;

    bios_hdd_get_geometry(&bios_geometry);
    geometry->total_sectors = bios_geometry.total_sectors;
    geometry->cylinders = bios_geometry.cylinders;
    geometry->heads = bios_geometry.heads;
    geometry->sectors_per_track = bios_geometry.sectors_per_track;
}

static unsigned int
legacy_memory_extended_usable_end_cb(unsigned int total_bytes) {
    return bios_memory_extended_usable_end(total_bytes);
}

static unsigned int legacy_memory_e820_entry_count_cb(unsigned int total_bytes) {
    return bios_memory_e820_entry_count(total_bytes);
}

static int legacy_memory_e820_get_entry_cb(unsigned int total_bytes,
                                           unsigned int index,
                                           struct legacy_e820_entry* entry) {
    struct e820_entry bios_entry;

    if (bios_memory_e820_get_entry(total_bytes, index, &bios_entry) != 0) {
        return -1;
    }
    entry->base_low = bios_entry.base_low;
    entry->base_high = bios_entry.base_high;
    entry->length_low = bios_entry.length_low;
    entry->length_high = bios_entry.length_high;
    entry->type = bios_entry.type;
    return 0;
}

static void linux_hdd_get_geometry_cb(
    struct linux_loader_hdd_geometry* geometry) {
    struct bios_hdd_geometry bios_geometry;

    bios_hdd_get_geometry(&bios_geometry);
    geometry->total_sectors = bios_geometry.total_sectors;
    geometry->cylinders = bios_geometry.cylinders;
    geometry->heads = bios_geometry.heads;
    geometry->sectors_per_track = bios_geometry.sectors_per_track;
}

static int linux_memory_e820_get_entry_cb(unsigned int total_bytes,
                                          unsigned int index,
                                          struct linux_loader_e820_entry* entry) {
    struct e820_entry bios_entry;

    if (bios_memory_e820_get_entry(total_bytes, index, &bios_entry) != 0) {
        return -1;
    }
    entry->base_low = bios_entry.base_low;
    entry->base_high = bios_entry.base_high;
    entry->length_low = bios_entry.length_low;
    entry->length_high = bios_entry.length_high;
    entry->type = bios_entry.type;
    return 0;
}

static void install_legacy_platform_ops(void) {
    struct legacy_platform_ops ops = {0};

    ops.serial_write_char = serial_write_char;
    ops.serial_write_string = serial_write_string;
    ops.serial_write_hex8 = serial_write_hex8;
    ops.serial_write_hex16 = serial_write_hex16;
    ops.serial_write_hex32 = serial_write_hex32;
    ops.serial_write_u32 = serial_write_u32;
    ops.hdd_is_present = bios_hdd_is_present;
    ops.hdd_current_kind = bios_hdd_current_kind;
    ops.hdd_select_kind = bios_hdd_select_kind;
    ops.hdd_get_geometry = legacy_hdd_get_geometry_cb;
    ops.hdd_read_sectors = bios_hdd_read_sectors;
    ops.hdd_load_mbr_boot_sector = bios_hdd_load_mbr_boot_sector;
    ops.rtc_read_time_bcd = bios_rtc_read_time_bcd;
    ops.rtc_read_date_bcd = bios_rtc_read_date_bcd;
    ops.rtc_set_time_bcd = bios_rtc_set_time_bcd;
    ops.rtc_set_date_bcd = bios_rtc_set_date_bcd;
    ops.memory_extended_usable_end = legacy_memory_extended_usable_end_cb;
    ops.memory_e820_entry_count = legacy_memory_e820_entry_count_cb;
    ops.memory_e820_get_entry = legacy_memory_e820_get_entry_cb;
    legacy_platform_init(&ops);
}

static void prepare_linux_platform(void) {
    bios_rtc_prepare_for_linux(nvram_enable_extended_cmos);
    acpi_install_for_linux();
}

static void fill_linux_loader_config(struct linux_loader_config* config) {
    config->total_bytes = bios_total_bytes_global;
    config->boot_priority = bios_boot_priority;
    config->vmlinux_partition = bios_linux_vmlinux_partition;
    config->enable_serial_console =
        (bios_nvram_flags0 & BIOS_NVRAM_FLAGS0_SERIAL_CONSOLE) != 0u ? 1u : 0u;
    config->enable_vesa_1024_768 =
        (bios_nvram_flags0 & BIOS_NVRAM_FLAGS0_VESA_1024_768) != 0u ? 1u : 0u;
    config->cmdline_suffix = bios_linux_cmdline_suffix;
    config->prepare_platform = prepare_linux_platform;
    config->init_vgabios = init_vgabios_for_linux;
    config->record_boot_success = nvram_record_boot_success;
    config->vbe_mode_info_buffer = legacy_vbe_mode_info_buffer;
    config->vbe_mode_info_pm32 = bios_call_vbe_mode_info_pm32;
    config->vbe_set_mode_pm32 = bios_call_vbe_set_mode_pm32;
    config->serial_write_string = serial_write_string;
    config->serial_write_hex8 = serial_write_hex8;
    config->serial_write_hex16 = serial_write_hex16;
    config->serial_write_hex32 = serial_write_hex32;
    config->serial_write_u32 = serial_write_u32;
    config->hdd_is_present = bios_hdd_is_present;
    config->hdd_current_kind = bios_hdd_current_kind;
    config->hdd_select_kind = bios_hdd_select_kind;
    config->hdd_get_geometry = linux_hdd_get_geometry_cb;
    config->hdd_read_sectors = bios_hdd_read_sectors;
    config->memory_extended_usable_end = bios_memory_extended_usable_end;
    config->memory_e820_entry_count = bios_memory_e820_entry_count;
    config->memory_e820_get_entry = linux_memory_e820_get_entry_cb;
}

static void install_bios_thunks(void) {
    struct legacy_runtime_config config;

    config.total_bytes = bios_total_bytes_global;
    config.floppy_present = legacy_floppy_present();
    config.hdd_present = bios_hdd_is_present();
    config.base_mem_kb = bios_dos_base_mem_kb;
    config.ebda_segment = bios_ebda_segment;
    config.boot_priority = bios_boot_priority;
    config.record_boot_success = nvram_record_boot_success;
    config.boot_pm32 = bios_boot_freedos_pm32;
    config.install_shadow = install_bios_shadow;
    legacy_runtime_init(&config);
}

static void prepare_boot_sector(void) {
    bios_boot_drive = legacy_prepare_boot_sector(
        bios_boot_priority, nvram_record_boot_success);
}

static void install_boot_drive(void) {
    legacy_install_boot_drive(bios_boot_drive);
}

static unsigned int bios_top_reserved_base(void) {
    return bios_memory_top_reserved_base(bios_total_bytes_global);
}

static unsigned int bios_pm_stack_top(void) {
    if (bios_shared_service_global != 0 &&
        bios_shared_service_global->stack_top != 0u) {
        return bios_shared_service_global->stack_top;
    }
    if (bios_total_bytes_global >= 0x00300000u) {
        return (bios_total_bytes_global & ~0xfffu) - 0x1000u;
    }
    return 0x001ff000u;
}

static void install_pm_stack_top(void) {
    legacy_install_pm_stack_top(bios_pm_stack_top());
}

static void acpi_clear_pm_events(unsigned int pm1_evt, unsigned int gpe0,
                                 unsigned int gpe0_len) {
    unsigned int half;
    unsigned int i;

    if (pm1_evt != 0u) {
        outw((unsigned short)(pm1_evt + 2u), 0x0000u);
        outw((unsigned short)pm1_evt, 0xffffu);
        (void)inw((unsigned short)pm1_evt);
    }

    if (gpe0 == 0u || gpe0_len < 2u) {
        return;
    }

    half = gpe0_len / 2u;
    for (i = 0; i < half; ++i) {
        outb((unsigned short)(gpe0 + half + i), 0x00u);
    }
    for (i = 0; i < half; ++i) {
        outb((unsigned short)(gpe0 + i), 0xffu);
    }
    (void)inb((unsigned short)gpe0);

    serial_write_string("ACPI PM sts=");
    serial_write_hex16(inw((unsigned short)pm1_evt));
    serial_write_string(" en=");
    serial_write_hex16(inw((unsigned short)(pm1_evt + 2u)));
    serial_write_string(" gpe=");
    serial_write_hex8(inb((unsigned short)gpe0));
    serial_write_string("/");
    serial_write_hex8(inb((unsigned short)(gpe0 + half)));
    serial_write_string("\r\n");
}

static void acpi_install_for_linux(void) {
    unsigned short cnt;

    if (bios_rsdp_linear_global == 0u) {
        serial_write_string("ACPI tables missing\r\n");
        return;
    }
    serial_write_string("ACPI RSDP=");
    serial_write_hex32(bios_rsdp_linear_global);
    serial_write_string("\r\n");
    acpi_clear_pm_events(bios_acpi_pm1_evt_global, bios_acpi_gpe0_global,
                         bios_acpi_gpe0_len_global);
    if ((bios_acpi_flags_global & SHARED_BOOT_ACPI_FLAG_ENABLE_SCI) != 0u &&
        bios_acpi_pm1_cnt_global != 0u) {
        cnt = inw((unsigned short)bios_acpi_pm1_cnt_global);
        if ((cnt & ACPI_PM1_CNT_SCI_EN) == 0u) {
            outw((unsigned short)bios_acpi_pm1_cnt_global,
                 (unsigned short)(cnt | ACPI_PM1_CNT_SCI_EN));
            cnt = inw((unsigned short)bios_acpi_pm1_cnt_global);
        }
        serial_write_string("ACPI PM1 cnt=");
        serial_write_hex16(cnt);
        serial_write_string("\r\n");
    }
}

static void run_test_elf_blob(void) {
    typedef unsigned int (*test_elf_entry_fn)(unsigned int, unsigned int,
                                             unsigned int, unsigned int);
    blob_expand_fn expand = bios_blob_expand_fn();
    void* blob_stage = bios_blob_stage_ptr();
    struct blob_status status;
    struct linux_loader_config linux_config = {0};
    unsigned char* image = (unsigned char*)LINUX_LOADER_TEST_ELF_IMAGE_LINEAR;
    unsigned int entry_phys = 0u;
    unsigned int rc;
    int expand_rc;

    if (bios_test_elf_blob_linear_global == 0u) {
        serial_write_string("No test ELF blob\r\n");
        return;
    }
    if (expand == 0 || blob_stage == 0) {
        serial_write_string("Test ELF blob service missing\r\n");
        return;
    }

    serial_write_string("Run ROM test ELF...\r\n");
    expand_rc = expand((const void*)bios_test_elf_blob_linear_global,
                       blob_stage, image, LINUX_LOADER_TEST_ELF_IMAGE_CAPACITY,
                       &status, bios_total_bytes_global);
    if (expand_rc != 0) {
        serial_write_string("Test ELF blob failed rc=");
        serial_write_hex8((unsigned char)expand_rc);
        serial_write_string(" block=");
        serial_write_hex32(status.block);
        serial_write_string("\r\n");
        return;
    }

    fill_linux_loader_config(&linux_config);
    if (linux_loader_load_elf_image(&linux_config, image, status.output_size,
                                    &entry_phys) != 0) {
        return;
    }

    storage_scan(bios_total_bytes_global);
    install_bios_thunks();
    install_boot_drive();
    install_pm_stack_top();
    install_vgabios_shadow();
    linux_loader_prepare_boot_params(&linux_config, entry_phys, 0u, 0u);

    serial_write_string("Call test ELF entry=");
    serial_write_hex32(entry_phys);
    serial_write_string(" params=");
    serial_write_hex32(LINUX_LOADER_BOOT_PARAMS);
    serial_write_string("\r\n");
    cpu_serialize();
    rc = ((test_elf_entry_fn)entry_phys)(
        LINUX_LOADER_BOOT_PARAMS, LINUX_LOADER_RSDP_LINEAR,
        bios_acpi_pm1_evt_global, bios_acpi_pm1_cnt_global);
    cpu_serialize();
    serial_write_string("Test ELF returned ");
    serial_write_hex32(rc);
    serial_write_string("\r\n");
}

static void nvram_record_boot_success(unsigned char kind) {
    if (bios_boot_priority != BIOS_NVRAM_BOOT_PRIORITY_AUTO) {
        return;
    }
    if (kind != BIOS_HDD_KIND_IDE && kind != BIOS_HDD_KIND_USB) {
        return;
    }
    bios_boot_priority = kind;
    nvram_save_boot_priority(kind);
    serial_write_string("boot priority learned=");
    serial_write_u32(kind);
    serial_write_string("\r\n");
}

static int try_boot_linux(void) {
    struct linux_loader_config config = {0};

    fill_linux_loader_config(&config);
    return linux_loader_try_boot(&config);
}

static unsigned int tsc_low(void) {
    unsigned int value;
    __asm__ volatile("rdtsc" : "=a"(value) : : "edx");
    return value;
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
        base[offset] = 0x13579bdfu ^ offset;
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
    bw = bandwidth_read_x100(l1, 16u * 1024u, 2048u, &checksum);
    serial_write_string("L1  16KiB: ");
    serial_write_fixed2(bw);
    serial_write_string(" (");
    serial_write_hex16((unsigned short)checksum);
    serial_write_string(")\r\n");

    bw = bandwidth_read_x100(l2, 256u * 1024u, 128u, &checksum);
    serial_write_string("L2 256KiB: ");
    serial_write_fixed2(bw);
    serial_write_string(" (");
    serial_write_hex16((unsigned short)checksum);
    serial_write_string(")\r\n");

    bw = bandwidth_read_x100(dram, 4u * 1024u * 1024u, 8u, &checksum);
    serial_write_string("DRAM   4MiB: ");
    serial_write_fixed2(bw);
    serial_write_string(" (");
    serial_write_hex16((unsigned short)checksum);
    serial_write_string(")\r\n");
}

static unsigned int bios_payload_blob_ptr(unsigned int payload_id) {
    struct shared_payload_entry* payload =
        shared_payload_find(bios_shared_service_global, payload_id);

    if (payload == 0) {
        return 0u;
    }
    return payload->blob_ptr;
}

static void bios_load_stage_context(unsigned int total_bytes) {
    struct shared_boot_context* boot_ctx;
    unsigned int blob;

    bios_total_bytes_global = total_bytes;
    bios_shared_service_global = shared_service_from_total(total_bytes);
    bios_vgabios_blob_linear_global = 0u;
    bios_test_elf_blob_linear_global = 0u;
    bios_rsdp_linear_global = 0u;
    bios_acpi_pm1_evt_global = 0u;
    bios_acpi_pm1_cnt_global = 0u;
    bios_acpi_gpe0_global = 0u;
    bios_acpi_gpe0_len_global = 0u;
    bios_acpi_flags_global = 0u;
    bios_maintenance_requested = 0u;
    bios_shadow_ready = 0u;

    boot_ctx = shared_boot_context(bios_shared_service_global);
    if (boot_ctx != 0) {
        if ((boot_ctx->flags & SHARED_BOOT_FLAG_MAINTENANCE_REQUESTED) != 0u) {
            bios_maintenance_requested = 1u;
        }
        if ((boot_ctx->flags & SHARED_BOOT_FLAG_SHADOW_READY) != 0u) {
            bios_shadow_ready = 1u;
        }
        bios_rsdp_linear_global = boot_ctx->rsdp_linear;
        bios_acpi_pm1_evt_global = boot_ctx->acpi_pm1_evt;
        bios_acpi_pm1_cnt_global = boot_ctx->acpi_pm1_cnt;
        bios_acpi_gpe0_global = boot_ctx->acpi_gpe0;
        bios_acpi_gpe0_len_global = boot_ctx->acpi_gpe0_len;
        bios_acpi_flags_global = boot_ctx->acpi_flags;
    }

    blob = bios_payload_blob_ptr(SHARED_PAYLOAD_ID_VGABIOS);
    if (blob != 0u) {
        bios_vgabios_blob_linear_global = blob;
    }
    blob = bios_payload_blob_ptr(SHARED_PAYLOAD_ID_TEST_ELF);
    if (blob != 0u) {
        bios_test_elf_blob_linear_global = blob;
    }

}

void postcar_resume(unsigned int total_bytes, unsigned int aux_blob_linear) {
    volatile unsigned int stack_cookie = 0x13579bdfu;

    (void)aux_blob_linear;
    bios_load_stage_context(total_bytes);
    storage_set_scratch_base(bios_top_reserved_base());
    nvram_load_settings();
    install_legacy_platform_ops();
    legacy_floppy_probe();
    outb(0x80, POST_DRAM_STACK);
    serial_write_string("stage3 @ 00200000\r\n");
    serial_write_string("post-CAR ok\r\n");
    serial_write_string("DRAM stack @ ");
    serial_write_hex16((unsigned short)(((unsigned int)&stack_cookie) >> 16));
    serial_write_hex16((unsigned short)((unsigned int)&stack_cookie));
    serial_write_string("\r\n");
    serial_write_string("Usable DRAM: ");
    serial_write_u32(total_bytes >> 10);
    serial_write_string("K\r\n");
    bios_memtest_run_optional(bios_enable_memtest, bios_total_bytes_global,
                              bios_shared_service_global);
    if (bios_run_test_blob != 0u) {
        nvram_consume_test_blob_request();
        run_test_elf_blob();
        serial_write_string("Test blob halted\r\n");
        for (;;) {
            __asm__ volatile("hlt");
        }
    }
    if (bios_maintenance_requested != 0u) {
        maintenance_prompt();
    }
    storage_scan(total_bytes);
    install_bios_thunks();
    install_boot_drive();
    install_pm_stack_top();
    install_vgabios_shadow();
    serial_write_string("IVT thunks installed @ ");
    serial_write_hex32(LEGACY_THUNK_RUNTIME_BASE);
    serial_write_string("\r\n");
    if (try_boot_linux()) {
        for (;;) {
            __asm__ volatile("hlt");
        }
    }
    prepare_boot_sector();
    install_boot_drive();
    serial_write_string("Booting drive=");
    serial_write_hex8(bios_boot_drive);
    serial_write_string("...\r\n");
    bios_boot_freedos_pm32();
    serial_write_string("FreeDOS returned\r\n");
    bandwidth_benchmarks(total_bytes);
    for (;;) {
        __asm__ volatile("hlt");
    }
}

void bios32_entry_c(unsigned int total_bytes, unsigned int aux_blob_linear) {
    zero_bss();
    postcar_resume(total_bytes, aux_blob_linear);
}
