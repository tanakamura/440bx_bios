#include "app/linux_loader/linux_loader.h"

int app_entry(const struct app_boot_context* ctx) {
    return linux_loader_try_boot(ctx);
}
