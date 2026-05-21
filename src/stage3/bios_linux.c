#include "bios_linux.h"

#include "bios_app_loader.h"
#include "bios_app_platform.h"
#include "blob.h"

typedef int (*linux_loader_app_entry_fn)(
    const struct linux_loader_config* loader);

int bios_linux_try_boot(const struct bios_linux_config* config) {
    struct linux_loader_config loader = {0};

    bios_app_platform_fill(&loader.platform, config->stage, config->settings);
    loader.runtime_protect_base = BIOS_LOAD_LINEAR;
    loader.runtime_protect_size = BIOS_LOAD_CAPACITY;
    if (config->stage->linux_loader_blob_linear == 0u) {
        return 0;
    }

    {
        unsigned int load_addr = bios_app_load(
            config->stage, SHARED_PAYLOAD_ID_LINUX_LOADER_APP, "Linux");
        if (load_addr != 0u) {
            return ((linux_loader_app_entry_fn)load_addr)(&loader);
        }
    }
    return 0;
}
