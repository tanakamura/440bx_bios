#include "app/legacy/legacy_runtime.h"

#include "app/legacy/legacy_floppy.h"
#include "app/legacy/legacy_timer.h"

void legacy_runtime_fill_exports(struct legacy_app_exports* exports) {
    if (exports == 0) {
        return;
    }
    exports->magic = LEGACY_APP_EXPORTS_MAGIC;
    exports->version = LEGACY_APP_EXPORTS_VERSION;
    exports->size = sizeof(*exports);
    exports->prepare_boot_sector = legacy_prepare_boot_sector;
    exports->install_boot_drive = legacy_install_boot_drive;
    exports->install_pm_stack_top = legacy_install_pm_stack_top;
}

void legacy_runtime_init(const struct legacy_runtime_config* config) {
    struct legacy_service_context context;
    unsigned int floppy_dpt_linear;

    legacy_platform_init(&config->platform_ops);
    legacy_floppy_probe();
    floppy_dpt_linear = legacy_install_bios_thunks(
        legacy_floppy_present(), config->hdd_present, config->base_mem_kb,
        config->ebda_segment, config->install_shadow, 0);

    context.total_bytes = config->total_bytes;
    context.floppy_dpt_linear = floppy_dpt_linear;
    context.base_mem_kb = config->base_mem_kb;
    context.boot_priority = config->boot_priority;
    context.record_boot_success = config->record_boot_success;
    context.boot_pm32 = config->boot_pm32;
    legacy_service_init(&context);
    legacy_timer_init();
    legacy_runtime_fill_exports(config->exports);
}
