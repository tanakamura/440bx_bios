#include "bios_legacy.h"

#include "bios_app_loader.h"
#include "bios_app_platform.h"
#include "bios_direct_thunk.h"
#include "bios_rm_fallback.h"
#include "bios_serial.h"
#include "bios_settings.h"
#include "legacy_app_abi.h"

static const unsigned short bios_ebda_segment = 0x0000u;
static const unsigned short bios_dos_base_mem_kb = 640u;

typedef void (*legacy_app_entry_fn)(const struct legacy_runtime_config* config);

static struct legacy_app_exports bios_legacy_exports;

void bios_legacy_install_runtime(
    const struct bios_stage_context* stage,
    const struct bios_settings* settings) {
    struct legacy_runtime_config config;

    config.base_mem_kb = bios_dos_base_mem_kb;
    config.ebda_segment = bios_ebda_segment;
    bios_app_platform_fill(&config.platform, stage, settings);
    config.exports = &bios_legacy_exports;
    bios_legacy_exports = (struct legacy_app_exports){0};

    if (stage->legacy_app_blob_linear != 0u) {
        unsigned int load_addr =
            bios_app_load(stage, SHARED_PAYLOAD_ID_LEGACY_APP, "Legacy");
        if (load_addr != 0u) {
            ((legacy_app_entry_fn)load_addr)(&config);
            return;
        }
    }

    serial_write_string("Legacy app missing; direct thunk only\r\n");
    bios_rm_fallback_install();
}

static int legacy_exports_ready(void) {
    return bios_legacy_exports.magic == LEGACY_APP_EXPORTS_MAGIC &&
           bios_legacy_exports.version == LEGACY_APP_EXPORTS_VERSION &&
           bios_legacy_exports.size >= sizeof(bios_legacy_exports);
}

unsigned char bios_legacy_prepare_boot_sector(void) {
    if (legacy_exports_ready() &&
        bios_legacy_exports.prepare_boot_sector != 0) {
        return bios_legacy_exports.prepare_boot_sector();
    }
    serial_write_string("Legacy app missing for boot\r\n");
    for (;;) {
        __asm__ volatile("hlt");
    }
}

void bios_legacy_install_boot_drive(unsigned char boot_drive) {
    if (legacy_exports_ready() && bios_legacy_exports.install_boot_drive != 0) {
        bios_legacy_exports.install_boot_drive(boot_drive);
        return;
    }
    bios_direct_thunk_install_boot_drive(boot_drive);
}

void bios_legacy_install_pm_stack_top(unsigned int pm_stack_top) {
    if (legacy_exports_ready() &&
        bios_legacy_exports.install_pm_stack_top != 0) {
        bios_legacy_exports.install_pm_stack_top(pm_stack_top);
        return;
    }
    bios_direct_thunk_install_pm_stack_top(pm_stack_top);
}
