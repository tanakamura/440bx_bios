#ifndef BIOS_DIRECT_THUNK_H
#define BIOS_DIRECT_THUNK_H

#define BIOS_DIRECT_THUNK_RUNTIME_BASE 0x000fe000u

typedef void (*bios_direct_thunk_void_fn)(void);

void bios_direct_thunk_install(bios_direct_thunk_void_fn install_shadow);
void bios_direct_thunk_install_boot_drive(unsigned char boot_drive);
void bios_direct_thunk_install_pm_stack_top(unsigned int stack_top);
unsigned char* bios_direct_thunk_vbe_mode_info_buffer(void);

#endif
