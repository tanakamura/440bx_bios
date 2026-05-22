#include "app_boot.h"

#include "app_loader.h"

struct app_boot_context* app_boot_context_alloc(unsigned int total_bytes,
                                                struct shared_service_table* shared) {
    shared_heap_alloc_fn alloc;

    if (shared == 0 || shared->heap_alloc == 0u) {
        return 0;
    }
    alloc = (shared_heap_alloc_fn)shared->heap_alloc;
    return (struct app_boot_context*)alloc(total_bytes,
                                           sizeof(struct app_boot_context));
}

void app_boot_context_init(struct app_boot_context* ctx, unsigned int app_id) {
    if (ctx == 0) {
        return;
    }
    *ctx = (struct app_boot_context){0};
    ctx->abi_magic = APP_BOOT_ABI_MAGIC;
    ctx->abi_version = APP_BOOT_ABI_VERSION;
    ctx->app_id = app_id;
}

int app_boot_run(blob_load_fn load, unsigned int total_bytes,
                 unsigned int payload_id, const char* label,
                 const struct app_boot_context* ctx) {
    unsigned int load_addr;

    if (ctx == 0) {
        return APP_BOOT_RESULT_FALLBACK;
    }
    load_addr = app_load_payload(load, total_bytes, payload_id, label);
    if (load_addr == 0u) {
        return APP_BOOT_RESULT_FALLBACK;
    }
    return ((app_entry_fn)load_addr)(ctx);
}
