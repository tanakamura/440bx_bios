#ifndef LEGACY_RUNTIME_H
#define LEGACY_RUNTIME_H

#include "app/legacy/legacy_boot.h"
#include "app/legacy/legacy_service.h"
#include "app/legacy/legacy_thunk.h"

struct legacy_runtime_config {
    unsigned int total_bytes;
    int hdd_present;
    unsigned short base_mem_kb;
    unsigned short ebda_segment;
    unsigned char boot_priority;
    legacy_boot_record_success_fn record_boot_success;
    legacy_boot_pm32_fn boot_pm32;
    legacy_thunk_void_fn install_shadow;
};

void legacy_runtime_init(const struct legacy_runtime_config* config);

#endif
