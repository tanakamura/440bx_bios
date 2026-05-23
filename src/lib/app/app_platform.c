#include "app_platform.h"

void app_platform_init(struct app_platform_info* info) {
    *info = (struct app_platform_info){0};
}

void app_platform_set_memory(struct app_platform_info* info,
                             unsigned int total_bytes) {
    info->total_bytes = total_bytes;
}

void app_platform_set_pci_snapshot(struct app_platform_info* info,
                                   unsigned int pci_snapshot_linear) {
    info->pci_snapshot_linear = pci_snapshot_linear;
}

void app_platform_set_storage_snapshot(struct app_platform_info* info,
                                       unsigned int storage_snapshot_linear) {
    info->storage_snapshot_linear = storage_snapshot_linear;
}

void app_platform_set_acpi(struct app_platform_info* info,
                           unsigned int rsdp_linear,
                           unsigned int pm1_evt,
                           unsigned int pm1_cnt,
                           unsigned int gpe0,
                           unsigned int gpe0_len,
                           unsigned int flags,
                           unsigned int table_base,
                           unsigned int table_size) {
    info->acpi_rsdp_linear = rsdp_linear;
    info->acpi_pm1_evt = pm1_evt;
    info->acpi_pm1_cnt = pm1_cnt;
    info->acpi_gpe0 = gpe0;
    info->acpi_gpe0_len = gpe0_len;
    info->acpi_flags = flags;
    info->acpi_table_base = table_base;
    info->acpi_table_size = table_size;
}

void app_platform_set_nvram(struct app_platform_info* info,
                            const struct app_nvram_snapshot* nvram) {
    if (nvram == 0) {
        return;
    }
    info->nvram = *nvram;
}
