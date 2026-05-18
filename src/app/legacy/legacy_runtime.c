#include "app/legacy/legacy_runtime.h"

#include "app/legacy/legacy_timer.h"

void legacy_runtime_init(const struct legacy_runtime_config* config) {
    struct legacy_service_context context;
    unsigned int floppy_dpt_linear;

    floppy_dpt_linear = legacy_install_bios_thunks(
        config->floppy_present, config->hdd_present, config->base_mem_kb,
        config->ebda_segment, config->install_shadow, 0);

    context.total_bytes = config->total_bytes;
    context.floppy_dpt_linear = floppy_dpt_linear;
    context.base_mem_kb = config->base_mem_kb;
    context.boot_priority = config->boot_priority;
    context.record_boot_success = config->record_boot_success;
    context.boot_pm32 = config->boot_pm32;
    legacy_service_init(&context);
    legacy_timer_init();
}
