#include "app/linux_loader/linux_loader_stage3.h"

#include "app_boot.h"
#include "app_loader.h"
#include "blob.h"
#include "linux_loader_abi.h"

int linux_loader_stage3_try_boot(const struct bios_stage_context* stage,
                                 const struct bios_settings* settings) {
    struct app_boot_context* ctx;

    if (stage->linux_loader_blob_linear == 0u) {
        return 0;
    }
    ctx = app_boot_context_alloc(stage);
    if (ctx == 0) {
        return 0;
    }
    app_boot_context_fill(ctx, APP_BOOT_ID_LINUX, stage, settings);
    return app_boot_run(stage, SHARED_PAYLOAD_ID_LINUX_LOADER_APP, "Linux",
                        ctx);
}
