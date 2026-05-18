#ifndef LEGACY_SERVICE_H
#define LEGACY_SERVICE_H

#include "app/legacy/legacy_boot.h"

typedef void (*legacy_boot_pm32_fn)(void);

struct legacy_service_context {
    unsigned int total_bytes;
    unsigned int floppy_dpt_linear;
    unsigned short base_mem_kb;
    unsigned char boot_priority;
    legacy_boot_record_success_fn record_boot_success;
    legacy_boot_pm32_fn boot_pm32;
};

void legacy_service_init(const struct legacy_service_context* context);

#endif
