#include "stage3.h"

#include "app_boot.h"
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

static struct bios_stage_context bios_stage;
static struct bios_settings bios_settings;

static unsigned int bios_top_reserved_base(void) {
    return bios_memory_top_reserved_base(bios_stage.total_bytes);
}

static void run_test_elf_payload(void) {
    selftest_stage3_run_elf_payload(&bios_stage, &bios_settings);
}

static int try_boot_linux(void) {
    return linux_loader_stage3_try_boot(&bios_stage, &bios_settings);
}

static void boot_legacy(void) {
    struct app_boot_context* ctx;
    int rc;

    if (bios_stage.legacy_app_blob_linear == 0u) {
        serial_write_string("Legacy app missing\r\n");
        for (;;) {
            __asm__ volatile("hlt");
        }
    }
    ctx = app_boot_context_alloc(&bios_stage);
    if (ctx == 0) {
        serial_write_string("Legacy ctx alloc failed\r\n");
        for (;;) {
            __asm__ volatile("hlt");
        }
    }
    app_boot_context_fill(ctx, APP_BOOT_ID_LEGACY, &bios_stage, &bios_settings);
    rc = app_boot_run(&bios_stage, SHARED_PAYLOAD_ID_LEGACY_APP, "Legacy", ctx);
    serial_write_string("Legacy returned rc=");
    serial_write_hex32((unsigned int)rc);
    serial_write_string("\r\n");
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
    if (try_boot_linux()) {
        for (;;) {
            __asm__ volatile("hlt");
        }
    }
    boot_legacy();
    bios_bandwidth_benchmarks(total_bytes);
    for (;;) {
        __asm__ volatile("hlt");
    }
}
