#ifndef LEGACY_BOOT_H
#define LEGACY_BOOT_H

#include "legacy_app_abi.h"

typedef legacy_app_record_success_fn legacy_boot_record_success_fn;

unsigned char legacy_prepare_boot_sector(
    unsigned char boot_priority, legacy_boot_record_success_fn record_success);

#endif
