#ifndef BIOS_LEGACY_H
#define BIOS_LEGACY_H

typedef void (*bios_legacy_void_fn)(void);
typedef void (*bios_legacy_record_boot_success_fn)(unsigned char kind);

struct bios_stage_context;

void bios_legacy_install_runtime(const struct bios_stage_context* stage,
                                 unsigned char boot_priority,
                                 bios_legacy_record_boot_success_fn record_boot_success,
                                 bios_legacy_void_fn boot_pm32,
                                 bios_legacy_void_fn install_shadow);
unsigned char bios_legacy_prepare_boot_sector(
    unsigned char boot_priority,
    bios_legacy_record_boot_success_fn record_boot_success);
void bios_legacy_install_boot_drive(unsigned char boot_drive);
void bios_legacy_install_pm_stack_top(unsigned int pm_stack_top);

#endif
