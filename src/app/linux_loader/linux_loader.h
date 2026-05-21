#ifndef LINUX_LOADER_H
#define LINUX_LOADER_H

#include "linux_loader_abi.h"

int linux_loader_try_boot(const struct linux_loader_config* config);
int linux_loader_load_elf_image(const struct linux_loader_config* config,
                                unsigned char* elf,
                                unsigned int image_size,
                                unsigned int* entry_phys);
void linux_loader_prepare_boot_params(
    const struct linux_loader_config* config, unsigned int entry_phys,
    unsigned int initrd_base, unsigned int initrd_size);

#endif
