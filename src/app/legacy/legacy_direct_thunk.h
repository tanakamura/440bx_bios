#ifndef LEGACY_DIRECT_THUNK_H
#define LEGACY_DIRECT_THUNK_H

#define LEGACY_DIRECT_THUNK_RUNTIME_BASE 0x000fe000u

typedef void (*legacy_direct_thunk_void_fn)(void);

void legacy_direct_thunk_install(legacy_direct_thunk_void_fn install_shadow);
void legacy_direct_thunk_install_boot_drive(unsigned char boot_drive);
void legacy_direct_thunk_install_pm_stack_top(unsigned int stack_top);
unsigned char* legacy_direct_thunk_vbe_mode_info_buffer(void);

#endif
