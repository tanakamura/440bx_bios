#include "app/legacy/legacy_runtime.h"

#include "bios_memory.h"
#include "bios_rtc.h"
#include "bios_serial.h"
#include "bios_storage.h"
#include "app/legacy/legacy_boot.h"
#include "app/legacy/legacy_floppy.h"
#include "app/legacy/legacy_platform.h"
#include "app/legacy/legacy_service.h"
#include "app/legacy/legacy_timer.h"
#include "app/legacy/legacy_thunk.h"
#include "app_runtime.h"

void legacy_runtime_init(const struct app_boot_context* ctx) {
    struct legacy_service_context context;
    unsigned int floppy_dpt_linear;
    static const unsigned short base_mem_kb = 640u;
    static const unsigned short ebda_segment = 0x0000u;

    legacy_boot_init(&ctx->platform);
    storage_set_scratch_base(bios_memory_top_reserved_base(
        ctx->platform.total_bytes));
    storage_scan(ctx->platform.total_bytes);
    legacy_floppy_probe();
    floppy_dpt_linear = legacy_install_bios_thunks(
        legacy_floppy_present(), bios_hdd_is_present(), base_mem_kb,
        ebda_segment, 0, 0);
    legacy_install_pm_stack_top(app_pm_stack_top(ctx->platform.total_bytes));

    context.platform = ctx->platform;
    context.floppy_dpt_linear = floppy_dpt_linear;
    context.base_mem_kb = base_mem_kb;
    legacy_service_init(&context);
    legacy_timer_init();
}
