#ifndef BIOS_SHADOW_H
#define BIOS_SHADOW_H

#include "blob.h"

typedef void (*bios_shadow_void_fn)(void);

void bios_shadow_install(unsigned char already_ready);
void bios_shadow_install_vgabios(unsigned int blob_linear,
                                 blob_load_fn load,
                                 unsigned int total_bytes);
void bios_shadow_init_vgabios(bios_shadow_void_fn init_pm32);

#endif
