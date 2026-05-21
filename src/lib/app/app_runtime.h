#ifndef APP_RUNTIME_H
#define APP_RUNTIME_H

#include "app_shadow.h"
#include "bios_stage_context.h"

unsigned int app_pm_stack_top(const struct bios_stage_context* stage);
void app_install_video_services(const struct bios_stage_context* stage,
                                app_shadow_vgabios_init_fn init_pm32);

#endif
