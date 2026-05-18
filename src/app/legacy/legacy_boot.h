#ifndef LEGACY_BOOT_H
#define LEGACY_BOOT_H

typedef void (*legacy_boot_record_success_fn)(unsigned char kind);

unsigned char legacy_prepare_boot_sector(
    unsigned char boot_priority, legacy_boot_record_success_fn record_success);

#endif
