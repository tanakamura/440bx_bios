#ifndef LINUX_LOADER_STAGE3_H
#define LINUX_LOADER_STAGE3_H

struct bios_stage_context;
struct bios_settings;

int linux_loader_stage3_try_boot(const struct bios_stage_context* stage,
                                 const struct bios_settings* settings);

#endif
