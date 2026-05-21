#ifndef SELFTEST_ABI_H
#define SELFTEST_ABI_H

#include "app_platform_abi.h"

struct selftest_runtime_info {
    struct app_platform_info platform;
    unsigned int boot_params;
};

#endif
