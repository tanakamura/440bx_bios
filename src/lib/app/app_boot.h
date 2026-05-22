#ifndef APP_BOOT_H
#define APP_BOOT_H

#include "app_boot_abi.h"
#include "bios_settings.h"
#include "bios_stage_context.h"

struct app_boot_context* app_boot_context_alloc(
    const struct bios_stage_context* stage);
void app_boot_context_fill(struct app_boot_context* ctx, unsigned int app_id,
                           const struct bios_stage_context* stage,
                           const struct bios_settings* settings);
int app_boot_run(const struct bios_stage_context* stage, unsigned int payload_id,
                 const char* label, const struct app_boot_context* ctx);

#endif
