#ifndef LEGACY_STAGE3_H
#define LEGACY_STAGE3_H

struct bios_stage_context;
struct bios_settings;

void legacy_stage3_install_runtime(const struct bios_stage_context* stage,
                                   const struct bios_settings* settings);
unsigned char legacy_stage3_prepare_boot_sector(void);
void legacy_stage3_install_boot_drive(unsigned char boot_drive);

#endif
