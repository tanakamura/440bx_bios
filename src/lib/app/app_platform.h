#ifndef APP_PLATFORM_H
#define APP_PLATFORM_H

#include "app_platform_abi.h"
#include "bios_settings.h"
#include "bios_stage_context.h"

void app_platform_fill(struct app_platform_info* info,
                       const struct bios_stage_context* stage,
                       const struct bios_settings* settings);

#endif
