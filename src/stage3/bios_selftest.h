#ifndef BIOS_SELFTEST_H
#define BIOS_SELFTEST_H

#include "bios_settings.h"
#include "bios_stage_context.h"

struct bios_selftest_config {
    const struct bios_stage_context* stage;
    const struct bios_settings* settings;
};

void bios_selftest_run_elf_payload(const struct bios_selftest_config* config);

#endif
