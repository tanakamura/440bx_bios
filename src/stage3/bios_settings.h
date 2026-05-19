#ifndef BIOS_SETTINGS_H
#define BIOS_SETTINGS_H

#include "bios_nvram.h"

struct bios_settings {
    unsigned char flags0;
    unsigned char boot_priority;
    unsigned char vmlinux_partition;
    unsigned char enable_memtest;
    unsigned char run_test_blob;
    char linux_cmdline_suffix[BIOS_NVRAM_CMDLINE_MAX];
};

void bios_settings_load(struct bios_settings* settings);
void bios_settings_consume_test_blob_request(struct bios_settings* settings);
void bios_settings_maintenance_prompt(struct bios_settings* settings);
void bios_settings_record_boot_success(struct bios_settings* settings,
                                       unsigned char kind);

#endif
