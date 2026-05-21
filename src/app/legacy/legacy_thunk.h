#ifndef LEGACY_THUNK_H
#define LEGACY_THUNK_H

#include "legacy_app_abi.h"

#define LEGACY_THUNK_RUNTIME_BASE 0x000fe000u

typedef void (*legacy_thunk_void_fn)(void);

unsigned int legacy_install_bios_thunks(
    int floppy_present, int hdd_present, unsigned short base_mem_kb,
    unsigned short ebda_segment, legacy_thunk_void_fn install_shadow,
    legacy_thunk_void_fn init_pit);
void legacy_install_boot_drive(unsigned char boot_drive);
void legacy_install_pm_stack_top(unsigned int stack_top);

#endif
