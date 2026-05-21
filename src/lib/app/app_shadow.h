#ifndef APP_SHADOW_H
#define APP_SHADOW_H

#include "blob.h"

typedef void (*app_shadow_vgabios_init_fn)(unsigned int bdf);

void app_shadow_install(unsigned char already_ready);
void app_shadow_install_vgabios(unsigned int blob_linear, blob_load_fn load,
                                unsigned int total_bytes);
void app_shadow_init_vgabios(app_shadow_vgabios_init_fn init_pm32);

#endif
