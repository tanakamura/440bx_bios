#ifndef LEGACY_RUNTIME_H
#define LEGACY_RUNTIME_H

#include "app/legacy/legacy_boot.h"
#include "app/legacy/legacy_platform.h"
#include "app/legacy/legacy_service.h"
#include "app/legacy/legacy_thunk.h"

#define LEGACY_APP_EXPORTS_MAGIC 0x4c415058u
#define LEGACY_APP_EXPORTS_VERSION 1u

struct legacy_app_exports {
    unsigned int magic;
    unsigned int version;
    unsigned int size;
    unsigned char (*prepare_boot_sector)(
        unsigned char boot_priority,
        legacy_boot_record_success_fn record_success);
    void (*install_boot_drive)(unsigned char boot_drive);
    void (*install_pm_stack_top)(unsigned int stack_top);
};

struct legacy_runtime_config {
    struct legacy_platform_ops platform_ops;
    unsigned int total_bytes;
    int hdd_present;
    unsigned short base_mem_kb;
    unsigned short ebda_segment;
    unsigned char boot_priority;
    legacy_boot_record_success_fn record_boot_success;
    legacy_boot_pm32_fn boot_pm32;
    legacy_thunk_void_fn install_shadow;
    struct legacy_app_exports* exports;
};

void legacy_runtime_init(const struct legacy_runtime_config* config);
void legacy_runtime_fill_exports(struct legacy_app_exports* exports);

#endif
