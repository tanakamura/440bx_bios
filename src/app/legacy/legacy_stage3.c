#include "app/legacy/legacy_stage3.h"

#include "app/legacy/legacy_direct_thunk.h"
#include "app/legacy/legacy_rm_fallback.h"
#include "app_loader.h"
#include "app_platform.h"
#include "bios_serial.h"
#include "legacy_app_abi.h"

static const unsigned short bios_ebda_segment = 0x0000u;
static const unsigned short bios_dos_base_mem_kb = 640u;

typedef void (*legacy_app_entry_fn)(const struct legacy_runtime_config* config);

static struct legacy_app_exports legacy_stage3_exports;

void legacy_stage3_install_runtime(const struct bios_stage_context* stage,
                                   const struct bios_settings* settings) {
    struct legacy_runtime_config config;

    config.base_mem_kb = bios_dos_base_mem_kb;
    config.ebda_segment = bios_ebda_segment;
    app_platform_fill(&config.platform, stage, settings);
    config.exports = &legacy_stage3_exports;
    legacy_stage3_exports = (struct legacy_app_exports){0};

    if (stage->legacy_app_blob_linear != 0u) {
        unsigned int load_addr =
            app_load_payload(stage, SHARED_PAYLOAD_ID_LEGACY_APP, "Legacy");
        if (load_addr != 0u) {
            ((legacy_app_entry_fn)load_addr)(&config);
            return;
        }
    }

    serial_write_string("Legacy app missing; direct thunk only\r\n");
    legacy_rm_fallback_install();
}

static int legacy_exports_ready(void) {
    return legacy_stage3_exports.magic == LEGACY_APP_EXPORTS_MAGIC &&
           legacy_stage3_exports.version == LEGACY_APP_EXPORTS_VERSION &&
           legacy_stage3_exports.size >= sizeof(legacy_stage3_exports);
}

unsigned char legacy_stage3_prepare_boot_sector(void) {
    if (legacy_exports_ready() &&
        legacy_stage3_exports.prepare_boot_sector != 0) {
        return legacy_stage3_exports.prepare_boot_sector();
    }
    serial_write_string("Legacy app missing for boot\r\n");
    for (;;) {
        __asm__ volatile("hlt");
    }
}

void legacy_stage3_install_boot_drive(unsigned char boot_drive) {
    if (legacy_exports_ready() &&
        legacy_stage3_exports.install_boot_drive != 0) {
        legacy_stage3_exports.install_boot_drive(boot_drive);
        return;
    }
    legacy_direct_thunk_install_boot_drive(boot_drive);
}
