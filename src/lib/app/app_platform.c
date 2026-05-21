#include "app_platform.h"

void app_platform_fill(struct app_platform_info* info,
                       const struct bios_stage_context* stage,
                       const struct bios_settings* settings) {
    unsigned int i;

    *info = (struct app_platform_info){0};
    info->total_bytes = stage->total_bytes;
    info->acpi_rsdp_linear = stage->rsdp_linear;
    info->acpi_pm1_evt = stage->acpi_pm1_evt;
    info->acpi_pm1_cnt = stage->acpi_pm1_cnt;
    info->acpi_gpe0 = stage->acpi_gpe0;
    info->acpi_gpe0_len = stage->acpi_gpe0_len;
    info->acpi_flags = stage->acpi_flags;
    info->acpi_table_base = stage->acpi_table_base;
    info->acpi_table_size = stage->acpi_table_size;
    info->nvram.flags0 = settings->flags0;
    info->nvram.boot_priority = settings->boot_priority;
    info->nvram.vmlinux_partition = settings->vmlinux_partition;
    info->nvram.enable_memtest = settings->enable_memtest;
    info->nvram.run_test_blob = settings->run_test_blob;
    for (i = 0u; i < BIOS_NVRAM_CMDLINE_MAX; ++i) {
        info->nvram.linux_cmdline_suffix[i] =
            settings->linux_cmdline_suffix[i];
        if (settings->linux_cmdline_suffix[i] == '\0') {
            break;
        }
    }
    info->nvram.linux_cmdline_suffix[BIOS_NVRAM_CMDLINE_MAX - 1u] = '\0';
}
