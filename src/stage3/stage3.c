#include "stage3.h"

#include "app/legacy/legacy_direct_thunk.h"
#include "app/legacy/legacy_stage3.h"
#include "app/linux_loader/linux_loader_stage3.h"
#include "app/selftest/s3test/selftest_stage3.h"
#include "bios_benchmark.h"
#include "bios_io.h"
#include "bios_memory.h"
#include "bios_memtest.h"
#include "bios_serial.h"
#include "bios_settings.h"
#include "bios_stage_context.h"
#include "bios_storage.h"
#include "post_code.h"

extern void bios_boot_freedos_pm32(void);

static struct bios_stage_context bios_stage;
static struct bios_settings bios_settings;
static unsigned char bios_boot_drive = 0x80u;

static void install_legacy_runtime(void) {
    legacy_stage3_install_runtime(&bios_stage, &bios_settings);
}

static void prepare_boot_sector(void) {
    bios_boot_drive = legacy_stage3_prepare_boot_sector();
}

static unsigned int bios_top_reserved_base(void) {
    return bios_memory_top_reserved_base(bios_stage.total_bytes);
}

static void run_test_elf_payload(void) {
    selftest_stage3_run_elf_payload(&bios_stage, &bios_settings);
}

static int try_boot_linux(void) {
    return linux_loader_stage3_try_boot(&bios_stage, &bios_settings);
}

void bios_stage3_run(unsigned int total_bytes) {
    volatile unsigned int stack_cookie = 0x13579bdfu;

    bios_stage_context_load(&bios_stage, total_bytes);
    storage_set_scratch_base(bios_top_reserved_base());
    bios_settings_load(&bios_settings,
                       shared_boot_context(bios_stage.shared_service));
    outb(0x80, POST_DRAM_STACK);
    serial_write_string("stage3 @ ");
    serial_write_hex32(BIOS_LOAD_LINEAR);
    serial_write_string("\r\n");
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
        run_test_elf_payload();
        serial_write_string("Test blob halted\r\n");
        for (;;) {
            __asm__ volatile("hlt");
        }
    }
    if (bios_stage.maintenance_requested != 0u) {
        bios_settings_maintenance_prompt(&bios_settings);
    }
    if (bios_stage.linux_loader_blob_linear != 0u) {
        storage_scan(total_bytes);
    }
    install_legacy_runtime();
    serial_write_string("IVT thunks installed @ ");
    serial_write_hex32(LEGACY_DIRECT_THUNK_RUNTIME_BASE);
    serial_write_string("\r\n");
    if (try_boot_linux()) {
        for (;;) {
            __asm__ volatile("hlt");
        }
    }
    prepare_boot_sector();
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
