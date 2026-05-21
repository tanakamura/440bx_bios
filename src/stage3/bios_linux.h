#ifndef BIOS_LINUX_H
#define BIOS_LINUX_H

#include "linux_loader_abi.h"
#include "bios_settings.h"
#include "bios_stage_context.h"

struct bios_linux_config {
    const struct bios_stage_context* stage;
    const struct bios_settings* settings;
};

int bios_linux_try_boot(const struct bios_linux_config* config);

#endif
