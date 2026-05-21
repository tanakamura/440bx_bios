#include "bios_benchmark.h"
#include "bios_io.h"
#include "bios_legacy.h"
#include "bios_linux.h"
#include "bios_memtest.h"
#include "bios_memory.h"
#include "bios_selftest.h"
#include "bios_serial.h"
#include "bios_settings.h"
#include "bios_shadow.h"
#include "stage3.h"
#include "bios_stage_context.h"
#include "bios_storage.h"
#include "app/legacy/legacy_thunk.h"
#include "post_code.h"

extern void bios_boot_freedos_pm32(void);
extern void bios_call_vgabios_init_pm32(void);
extern unsigned int bios_call_vbe_mode_info_pm32(unsigned int mode);
extern unsigned int bios_call_vbe_set_mode_pm32(unsigned int mode);

static struct bios_stage_context bios_stage;
static struct bios_settings bios_settings;
static unsigned char bios_boot_drive = 0x80u;

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

static void fill_bios_linux_config(struct bios_linux_config* config) {
    config->stage = &bios_stage;
    config->settings = &bios_settings;
    config->record_boot_success = nvram_record_boot_success;
    config->init_vgabios = init_vgabios_for_linux;
    config->vbe_mode_info_pm32 = bios_call_vbe_mode_info_pm32;
    config->vbe_set_mode_pm32 = bios_call_vbe_set_mode_pm32;
}

static void install_bios_thunks(void) {
    bios_legacy_install_runtime(bios_stage.total_bytes,
                                bios_settings.boot_priority,
                                nvram_record_boot_success,
                                bios_boot_freedos_pm32, install_bios_shadow);
}

static void prepare_boot_sector(void) {
    bios_boot_drive = bios_legacy_prepare_boot_sector(
        bios_settings.boot_priority, nvram_record_boot_success);
}

static void install_boot_drive(void) {
    bios_legacy_install_boot_drive(bios_boot_drive);
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
    bios_legacy_install_pm_stack_top(bios_pm_stack_top());
}

static void run_test_elf_blob(void) {
    struct bios_linux_config linux_platform = {0};
    struct bios_selftest_config selftest = {0};

    fill_bios_linux_config(&linux_platform);
    selftest.stage = &bios_stage;
    selftest.linux_config = &linux_platform;
    selftest.install_legacy_runtime = install_bios_thunks;
    selftest.install_boot_drive = install_boot_drive;
    selftest.install_pm_stack_top = install_pm_stack_top;
    selftest.install_vgabios_shadow = install_vgabios_shadow;
    bios_selftest_run_elf_blob(&selftest);
}

static void nvram_record_boot_success(unsigned char kind) {
    bios_settings_record_boot_success(&bios_settings, kind);
}

static int try_boot_linux(void) {
    struct bios_linux_config config = {0};

    fill_bios_linux_config(&config);
    return bios_linux_try_boot(&config);
}

void bios_stage3_run(unsigned int total_bytes, unsigned int aux_blob_linear) {
    volatile unsigned int stack_cookie = 0x13579bdfu;

    (void)aux_blob_linear;
    bios_stage_context_load(&bios_stage, total_bytes);
    storage_set_scratch_base(bios_top_reserved_base());
    bios_settings_load(&bios_settings);
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
