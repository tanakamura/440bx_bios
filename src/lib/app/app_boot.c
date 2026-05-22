#include "app_boot.h"

#include "app_loader.h"
#include "app_platform.h"
#include "blob.h"
#include "shared_service/service_table.h"

struct app_boot_context* app_boot_context_alloc(
    const struct bios_stage_context* stage) {
    shared_heap_alloc_fn alloc;

    if (stage == 0 || stage->shared_service == 0 ||
        stage->shared_service->heap_alloc == 0u) {
        return 0;
    }
    alloc = (shared_heap_alloc_fn)stage->shared_service->heap_alloc;
    return (struct app_boot_context*)alloc(stage->total_bytes, sizeof(struct app_boot_context));
}

void app_boot_context_fill(struct app_boot_context* ctx, unsigned int app_id,
                           const struct bios_stage_context* stage,
                           const struct bios_settings* settings) {
    if (ctx == 0 || stage == 0 || settings == 0) {
        return;
    }
    *ctx = (struct app_boot_context){0};
    ctx->abi_magic = APP_BOOT_ABI_MAGIC;
    ctx->abi_version = APP_BOOT_ABI_VERSION;
    ctx->app_id = app_id;
    app_platform_fill(&ctx->platform, stage, settings);
    ctx->runtime_base = BIOS_LOAD_LINEAR;
    ctx->runtime_size = BIOS_LOAD_CAPACITY;
    ctx->boot_params_linear = 0u;
    ctx->work_linear = APP_SLOT_LOAD_LINEAR;
    ctx->work_size = APP_SLOT_LOAD_CAPACITY;
}

int app_boot_run(const struct bios_stage_context* stage, unsigned int payload_id,
                 const char* label, const struct app_boot_context* ctx) {
    unsigned int load_addr;

    if (stage == 0 || ctx == 0) {
        return APP_BOOT_RESULT_FALLBACK;
    }
    load_addr = app_load_payload(stage, payload_id, label);
    if (load_addr == 0u) {
        return APP_BOOT_RESULT_FALLBACK;
    }
    return ((app_entry_fn)load_addr)(ctx);
}
