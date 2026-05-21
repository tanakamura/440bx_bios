#include "app/linux_loader/linux_loader_stage3.h"

#include "app_loader.h"
#include "app_platform.h"
#include "blob.h"
#include "linux_loader_abi.h"

typedef int (*linux_loader_app_entry_fn)(
    const struct linux_loader_config* loader);

int linux_loader_stage3_try_boot(const struct bios_stage_context* stage,
                                 const struct bios_settings* settings) {
    struct linux_loader_config loader = {0};

    app_platform_fill(&loader.platform, stage, settings);
    loader.runtime_protect_base = BIOS_LOAD_LINEAR;
    loader.runtime_protect_size = BIOS_LOAD_CAPACITY;
    if (stage->linux_loader_blob_linear == 0u) {
        return 0;
    }

    {
        unsigned int load_addr =
            app_load_payload(stage, SHARED_PAYLOAD_ID_LINUX_LOADER_APP, "Linux");
        if (load_addr != 0u) {
            return ((linux_loader_app_entry_fn)load_addr)(&loader);
        }
    }
    return 0;
}
