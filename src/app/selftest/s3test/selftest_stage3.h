#ifndef SELFTEST_STAGE3_H
#define SELFTEST_STAGE3_H

struct bios_stage_context;
struct bios_settings;

void selftest_stage3_run_elf_payload(const struct bios_stage_context* stage,
                                     const struct bios_settings* settings);

#endif
