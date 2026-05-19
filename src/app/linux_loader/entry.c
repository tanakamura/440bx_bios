#include "app/linux_loader/linux_loader.h"

int linux_loader_app_entry(const struct linux_loader_config* config) {
    return linux_loader_try_boot(config);
}
