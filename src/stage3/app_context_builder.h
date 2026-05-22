#ifndef STAGE3_APP_CONTEXT_BUILDER_H
#define STAGE3_APP_CONTEXT_BUILDER_H

#include "app_boot_abi.h"

struct bios_settings;
struct bios_stage_context;

struct app_boot_context* stage3_app_context_alloc_and_fill(
    const struct bios_stage_context* stage,
    const struct bios_settings* settings,
    unsigned int app_id);

#endif
