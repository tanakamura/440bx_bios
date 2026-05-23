#ifndef APP_PLATFORM_ABI_H
#define APP_PLATFORM_ABI_H

#include "bios_nvram.h"

struct app_nvram_snapshot {
    unsigned char flags0;
    unsigned char boot_priority;
    unsigned char vmlinux_partition;
    unsigned char enable_memtest;
    unsigned char run_test_blob;
    char linux_cmdline_suffix[BIOS_NVRAM_CMDLINE_MAX];
};

struct app_platform_info {
    unsigned int total_bytes;
    unsigned int pci_snapshot_linear;
    unsigned int storage_snapshot_linear;
    unsigned int acpi_rsdp_linear;
    unsigned int acpi_pm1_evt;
    unsigned int acpi_pm1_cnt;
    unsigned int acpi_gpe0;
    unsigned int acpi_gpe0_len;
    unsigned int acpi_flags;
    unsigned int acpi_table_base;
    unsigned int acpi_table_size;
    struct app_nvram_snapshot nvram;
};

#endif
