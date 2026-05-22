#ifndef LINUX_LOADER_H
#define LINUX_LOADER_H

#include "linux_loader_abi.h"

int linux_loader_try_boot(const struct app_boot_context* config);
int linux_loader_load_elf_image(const struct app_boot_context* config,
                                unsigned char* elf,
                                unsigned int image_size,
                                unsigned int* entry_phys);
void linux_loader_prepare_boot_params(
    const struct app_boot_context* config, unsigned int entry_phys,
    unsigned int initrd_base, unsigned int initrd_size);

#endif
