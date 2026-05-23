#ifndef APP_PLATFORM_H
#define APP_PLATFORM_H

#include "app_platform_abi.h"

void app_platform_init(struct app_platform_info* info);
void app_platform_set_memory(struct app_platform_info* info,
                             unsigned int total_bytes);
void app_platform_set_pci_snapshot(struct app_platform_info* info,
                                   unsigned int pci_snapshot_linear);
void app_platform_set_storage_snapshot(struct app_platform_info* info,
                                       unsigned int storage_snapshot_linear);
void app_platform_set_acpi(struct app_platform_info* info,
                           unsigned int rsdp_linear,
                           unsigned int pm1_evt,
                           unsigned int pm1_cnt,
                           unsigned int gpe0,
                           unsigned int gpe0_len,
                           unsigned int flags,
                           unsigned int table_base,
                           unsigned int table_size);
void app_platform_set_nvram(struct app_platform_info* info,
                            const struct app_nvram_snapshot* nvram);

#endif
