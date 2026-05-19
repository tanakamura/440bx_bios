#include "bios_acpi_runtime.h"
#include "bios_benchmark.h"
#include "bios_io.h"
#include "bios_memtest.h"
#include "bios_memory.h"
#include "bios_rtc.h"
#include "bios_serial.h"
#include "bios_settings.h"
#include "bios_shadow.h"
#include "bios_stage_context.h"
#include "bios_storage.h"
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

static struct bios_stage_context bios_stage;
static struct bios_settings bios_settings;
static const unsigned short bios_ebda_segment = 0x0000u;
static const unsigned short bios_dos_base_mem_kb = 640u;
static unsigned char bios_boot_drive = 0x80u;

static void zero_bss(void) {
    unsigned char* p = __bss_start;
    while (p < __bss_end) {
        *p++ = 0u;
    }
}

static void cpu_serialize(void) {
    __asm__ volatile("xorl %%eax, %%eax\n\tcpuid"
                     :
                     :
                     : "eax", "ebx", "ecx", "edx", "memory");
}

static void install_bios_shadow(void) {
    bios_shadow_install(bios_stage.shadow_ready);
}

static void install_vgabios_shadow(void) {
    bios_shadow_install_vgabios(
        bios_stage.vgabios_blob_linear,
        bios_stage_context_blob_expand(&bios_stage),
        bios_stage_context_blob_stage(&bios_stage), bios_stage.total_bytes);
}

static void init_vgabios_for_linux(void) {
    bios_shadow_init_vgabios(bios_call_vgabios_init_pm32);
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
    bios_rtc_prepare_for_linux(bios_nvram_enable_extended_cmos);
    bios_acpi_install_for_linux(
        bios_stage.rsdp_linear, bios_stage.acpi_pm1_evt,
        bios_stage.acpi_pm1_cnt, bios_stage.acpi_gpe0,
        bios_stage.acpi_gpe0_len, bios_stage.acpi_flags);
}

static void fill_linux_loader_config(struct linux_loader_config* config) {
    config->total_bytes = bios_stage.total_bytes;
    config->boot_priority = bios_settings.boot_priority;
    config->vmlinux_partition = bios_settings.vmlinux_partition;
    config->enable_serial_console =
        (bios_settings.flags0 & BIOS_NVRAM_FLAGS0_SERIAL_CONSOLE) != 0u ? 1u
                                                                        : 0u;
    config->enable_vesa_1024_768 =
        (bios_settings.flags0 & BIOS_NVRAM_FLAGS0_VESA_1024_768) != 0u ? 1u
                                                                       : 0u;
    config->cmdline_suffix = bios_settings.linux_cmdline_suffix;
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

    config.total_bytes = bios_stage.total_bytes;
    config.floppy_present = legacy_floppy_present();
    config.hdd_present = bios_hdd_is_present();
    config.base_mem_kb = bios_dos_base_mem_kb;
    config.ebda_segment = bios_ebda_segment;
    config.boot_priority = bios_settings.boot_priority;
    config.record_boot_success = nvram_record_boot_success;
    config.boot_pm32 = bios_boot_freedos_pm32;
    config.install_shadow = install_bios_shadow;
    legacy_runtime_init(&config);
}

static void prepare_boot_sector(void) {
    bios_boot_drive = legacy_prepare_boot_sector(
        bios_settings.boot_priority, nvram_record_boot_success);
}

static void install_boot_drive(void) {
    legacy_install_boot_drive(bios_boot_drive);
}

static unsigned int bios_top_reserved_base(void) {
    return bios_memory_top_reserved_base(bios_stage.total_bytes);
}

static unsigned int bios_pm_stack_top(void) {
    if (bios_stage.shared_service != 0 &&
        bios_stage.shared_service->stack_top != 0u) {
        return bios_stage.shared_service->stack_top;
    }
    if (bios_stage.total_bytes >= 0x00300000u) {
        return (bios_stage.total_bytes & ~0xfffu) - 0x1000u;
    }
    return 0x001ff000u;
}

static void install_pm_stack_top(void) {
    legacy_install_pm_stack_top(bios_pm_stack_top());
}

static void run_test_elf_blob(void) {
    typedef unsigned int (*test_elf_entry_fn)(unsigned int, unsigned int,
                                             unsigned int, unsigned int);
    blob_expand_fn expand = bios_stage_context_blob_expand(&bios_stage);
    void* blob_stage = bios_stage_context_blob_stage(&bios_stage);
    struct blob_status status;
    struct linux_loader_config linux_config = {0};
    unsigned char* image = (unsigned char*)LINUX_LOADER_TEST_ELF_IMAGE_LINEAR;
    unsigned int entry_phys = 0u;
    unsigned int rc;
    int expand_rc;

    if (bios_stage.test_elf_blob_linear == 0u) {
        serial_write_string("No test ELF blob\r\n");
        return;
    }
    if (expand == 0 || blob_stage == 0) {
        serial_write_string("Test ELF blob service missing\r\n");
        return;
    }

    serial_write_string("Run ROM test ELF...\r\n");
    expand_rc = expand((const void*)bios_stage.test_elf_blob_linear,
                       blob_stage, image, LINUX_LOADER_TEST_ELF_IMAGE_CAPACITY,
                       &status, bios_stage.total_bytes);
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

    storage_scan(bios_stage.total_bytes);
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
        bios_stage.acpi_pm1_evt, bios_stage.acpi_pm1_cnt);
    cpu_serialize();
    serial_write_string("Test ELF returned ");
    serial_write_hex32(rc);
    serial_write_string("\r\n");
}

static void nvram_record_boot_success(unsigned char kind) {
    bios_settings_record_boot_success(&bios_settings, kind);
}

static int try_boot_linux(void) {
    struct linux_loader_config config = {0};

    fill_linux_loader_config(&config);
    return linux_loader_try_boot(&config);
}

void postcar_resume(unsigned int total_bytes, unsigned int aux_blob_linear) {
    volatile unsigned int stack_cookie = 0x13579bdfu;

    (void)aux_blob_linear;
    bios_stage_context_load(&bios_stage, total_bytes);
    storage_set_scratch_base(bios_top_reserved_base());
    bios_settings_load(&bios_settings);
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
    bios_memtest_run_optional(bios_settings.enable_memtest,
                              bios_stage.total_bytes,
                              bios_stage.shared_service);
    if (bios_settings.run_test_blob != 0u) {
        bios_settings_consume_test_blob_request(&bios_settings);
        run_test_elf_blob();
        serial_write_string("Test blob halted\r\n");
        for (;;) {
            __asm__ volatile("hlt");
        }
    }
    if (bios_stage.maintenance_requested != 0u) {
        bios_settings_maintenance_prompt(&bios_settings);
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
    bios_bandwidth_benchmarks(total_bytes);
    for (;;) {
        __asm__ volatile("hlt");
    }
}

void bios32_entry_c(unsigned int total_bytes, unsigned int aux_blob_linear) {
    zero_bss();
    postcar_resume(total_bytes, aux_blob_linear);
}
