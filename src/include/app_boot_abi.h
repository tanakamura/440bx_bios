#ifndef APP_BOOT_ABI_H
#define APP_BOOT_ABI_H

#include "app_platform_abi.h"

#define APP_BOOT_ABI_MAGIC 0x41504254u
#define APP_BOOT_ABI_VERSION 1u

enum app_boot_id {
    APP_BOOT_ID_NONE = 0u,
    APP_BOOT_ID_LEGACY = 1u,
    APP_BOOT_ID_LINUX = 2u,
    APP_BOOT_ID_SELFTEST = 3u,
};

enum app_boot_result {
    APP_BOOT_RESULT_FALLBACK = -1,
    APP_BOOT_RESULT_OK = 0,
    APP_BOOT_RESULT_DONE = 1,
};

struct app_boot_context {
    unsigned int abi_magic;
    unsigned int abi_version;
    unsigned int app_id;
    struct app_platform_info platform;
    unsigned int runtime_base;
    unsigned int runtime_size;
    unsigned int boot_params_linear;
    unsigned int work_linear;
    unsigned int work_size;
};

typedef int (*app_entry_fn)(const struct app_boot_context* ctx);

#endif
