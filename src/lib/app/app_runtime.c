#include "app_runtime.h"

void app_install_video_services(const struct bios_stage_context* stage,
                                app_shadow_vgabios_init_fn init_pm32) {
    app_shadow_install(stage->shadow_ready);
    app_shadow_install_vgabios(stage->vgabios_blob_linear,
                               bios_stage_context_blob_load(stage),
                               stage->total_bytes);
    app_shadow_init_vgabios(init_pm32);
}

unsigned int app_pm_stack_top(const struct bios_stage_context* stage) {
    if (stage->shared_service != 0 && stage->shared_service->stack_top != 0u) {
        return stage->shared_service->stack_top;
    }
    if (stage->total_bytes >= 0x00300000u) {
        return (stage->total_bytes & ~0xfffu) - 0x1000u;
    }
    return 0x001ff000u;
}
