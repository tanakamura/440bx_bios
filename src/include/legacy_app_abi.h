#ifndef LEGACY_APP_ABI_H
#define LEGACY_APP_ABI_H

#include "app_platform_abi.h"

#define LEGACY_APP_EXPORTS_MAGIC 0x4c415058u
#define LEGACY_APP_EXPORTS_VERSION 1u

struct legacy_app_exports {
    unsigned int magic;
    unsigned int version;
    unsigned int size;
    unsigned char (*prepare_boot_sector)(void);
    void (*install_boot_drive)(unsigned char boot_drive);
    void (*install_pm_stack_top)(unsigned int stack_top);
};

struct legacy_runtime_config {
    unsigned short base_mem_kb;
    unsigned short ebda_segment;
    struct app_platform_info platform;
    struct legacy_app_exports* exports;
};

#endif
