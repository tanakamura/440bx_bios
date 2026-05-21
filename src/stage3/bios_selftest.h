#ifndef BIOS_SELFTEST_H
#define BIOS_SELFTEST_H

#include "bios_stage_context.h"

typedef void (*bios_selftest_void_fn)(void);

struct bios_selftest_config {
    const struct bios_stage_context* stage;
    bios_selftest_void_fn install_legacy_runtime;
    bios_selftest_void_fn install_boot_drive;
    bios_selftest_void_fn install_pm_stack_top;
    bios_selftest_void_fn install_vgabios_shadow;
};

void bios_selftest_run_elf_payload(const struct bios_selftest_config* config);

#endif
