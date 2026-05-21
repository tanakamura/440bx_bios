#ifndef LEGACY_BOOT_H
#define LEGACY_BOOT_H

#include "app_platform_abi.h"
#include "legacy_app_abi.h"

void legacy_boot_init(const struct app_platform_info* platform);
unsigned char legacy_prepare_boot_sector(void);

#endif
