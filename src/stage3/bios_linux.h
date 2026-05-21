#ifndef BIOS_LINUX_H
#define BIOS_LINUX_H

#include "linux_loader_abi.h"
#include "bios_settings.h"
#include "bios_stage_context.h"

struct bios_linux_config {
    const struct bios_stage_context* stage;
    const struct bios_settings* settings;
    void (*record_boot_success)(unsigned char kind);
    void (*init_vgabios)(void);
    unsigned int (*vbe_mode_info_pm32)(unsigned int mode);
    unsigned int (*vbe_set_mode_pm32)(unsigned int mode);
};

void bios_linux_fill_loader_config(struct linux_loader_config* loader,
                                   const struct bios_linux_config* config);
int bios_linux_try_boot(const struct bios_linux_config* config);

#endif
