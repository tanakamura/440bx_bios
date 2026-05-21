#ifndef LEGACY_SERVICE_H
#define LEGACY_SERVICE_H

#include "app_platform_abi.h"
#include "app/legacy/legacy_boot.h"

struct legacy_service_context {
    struct app_platform_info platform;
    unsigned int floppy_dpt_linear;
    unsigned short base_mem_kb;
};

void legacy_service_init(const struct legacy_service_context* context);

#endif
