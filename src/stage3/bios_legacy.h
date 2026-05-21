#ifndef BIOS_LEGACY_H
#define BIOS_LEGACY_H

struct bios_stage_context;
struct bios_settings;

void bios_legacy_install_runtime(const struct bios_stage_context* stage,
                                 const struct bios_settings* settings);
unsigned char bios_legacy_prepare_boot_sector(void);
void bios_legacy_install_boot_drive(unsigned char boot_drive);
void bios_legacy_install_pm_stack_top(unsigned int pm_stack_top);

#endif
