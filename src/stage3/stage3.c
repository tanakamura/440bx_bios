#include "stage3.h"

#include "app_boot.h"
#include "app_context_builder.h"
#include "bios_io.h"
#include "bios_memory.h"
#include "bios_memtest.h"
#include "bios_pci_bus.h"
#include "bios_serial.h"
#include "bios_settings.h"
#include "bios_stage_context.h"
#include "bios_storage.h"
#include "post_code.h"
#include "stage3/pci_snapshot.h"

static struct bios_stage_context bios_stage;
static struct bios_settings bios_settings;

static unsigned int bios_top_reserved_base(void) {
    return bios_memory_top_reserved_base(bios_stage.total_bytes);
}

static int run_app_payload(unsigned int app_blob_linear, unsigned int app_id,
                           unsigned int payload_id, const char* label) {
    struct app_boot_context* ctx;

    if (app_blob_linear == 0u) {
        return APP_BOOT_RESULT_FALLBACK;
    }
    ctx =
        stage3_app_context_alloc_and_fill(&bios_stage, &bios_settings, app_id);
    if (ctx == 0) {
        return APP_BOOT_RESULT_FALLBACK;
    }
    return app_boot_run(bios_stage_context_blob_load(&bios_stage),
                        bios_stage.total_bytes, payload_id, label, ctx);
}

static int try_boot_linux(void) {
    int rc =
        run_app_payload(bios_stage.linux_loader_blob_linear, APP_BOOT_ID_LINUX,
                        SHARED_PAYLOAD_ID_LINUX_LOADER_APP, "Linux");
    return rc > 0 ? 1 : 0;
}

static void boot_legacy(void) {
    int rc;

    if (bios_stage.legacy_app_blob_linear == 0u) {
        serial_write_string("Legacy app missing\r\n");
        for (;;) {
            __asm__ volatile("hlt");
        }
    }
    rc = run_app_payload(bios_stage.legacy_app_blob_linear, APP_BOOT_ID_LEGACY,
                         SHARED_PAYLOAD_ID_LEGACY_APP, "Legacy");
    serial_write_string("Legacy returned rc=");
    serial_write_hex32((unsigned int)rc);
    serial_write_string("\r\n");
}

void bios_stage3_run(unsigned int total_bytes) {
    volatile unsigned int stack_cookie = 0x13579bdfu;
    unsigned char selftest_only_profile;

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
    selftest_only_profile =
        (unsigned char)(bios_stage.selftest_app_blob_linear != 0u &&
                        bios_stage.legacy_app_blob_linear == 0u &&
                        bios_stage.linux_loader_blob_linear == 0u);
    bios_memtest_run_optional(bios_settings.enable_memtest,
                              bios_stage.total_bytes,
                              bios_stage.shared_service);
    pci_bus_enumerate_and_assign(
        total_bytes, shared_boot_context(bios_stage.shared_service));
    stage3_pci_snapshot_build(total_bytes, bios_stage.shared_service);
    storage_scan(total_bytes);
    storage_snapshot_export(total_bytes, bios_stage.shared_service);
    stage3_pci_snapshot_build(total_bytes, bios_stage.shared_service);

    if (bios_settings.run_test_blob != 0u || selftest_only_profile != 0u) {
        int rc;
        if (bios_settings.run_test_blob != 0u) {
            bios_settings_consume_test_blob_request(&bios_settings);
        }
        rc = run_app_payload(bios_stage.selftest_app_blob_linear,
                             APP_BOOT_ID_SELFTEST,
                             SHARED_PAYLOAD_ID_SELFTEST_APP, "Selftest");
        serial_write_string("Selftest returned rc=");
        serial_write_hex32((unsigned int)rc);
        serial_write_string("\r\n");
        for (;;) {
            __asm__ volatile("hlt");
        }
    }
    if (bios_stage.maintenance_requested != 0u) {
        bios_settings_maintenance_prompt(&bios_settings);
    }
    if (try_boot_linux()) {
        for (;;) {
            __asm__ volatile("hlt");
        }
    }
    boot_legacy();
    for (;;) {
        __asm__ volatile("hlt");
    }
}
