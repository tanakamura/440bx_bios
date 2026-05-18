#ifndef LINUX_LOADER_H
#define LINUX_LOADER_H

#define LINUX_LOADER_BOOT_PARAMS 0x00090000u
#define LINUX_LOADER_RSDP_LINEAR 0x0009fc00u
#define LINUX_LOADER_TEST_ELF_IMAGE_LINEAR 0x00280000u
#define LINUX_LOADER_TEST_ELF_IMAGE_CAPACITY 0x00040000u

typedef void (*linux_loader_void_fn)(void);
typedef void (*linux_loader_record_boot_success_fn)(unsigned char kind);
typedef unsigned char* (*linux_loader_buffer_fn)(void);
typedef unsigned int (*linux_loader_vbe_fn)(unsigned int mode);

struct linux_loader_config {
    unsigned int total_bytes;
    unsigned char boot_priority;
    unsigned char vmlinux_partition;
    unsigned char enable_serial_console;
    unsigned char enable_vesa_1024_768;
    const char* cmdline_suffix;
    linux_loader_void_fn prepare_platform;
    linux_loader_void_fn init_vgabios;
    linux_loader_record_boot_success_fn record_boot_success;
    linux_loader_buffer_fn vbe_mode_info_buffer;
    linux_loader_vbe_fn vbe_mode_info_pm32;
    linux_loader_vbe_fn vbe_set_mode_pm32;
};

int linux_loader_try_boot(const struct linux_loader_config* config);
int linux_loader_load_elf_image(const struct linux_loader_config* config,
                                unsigned char* elf,
                                unsigned int image_size,
                                unsigned int* entry_phys);
void linux_loader_prepare_boot_params(
    const struct linux_loader_config* config, unsigned int entry_phys,
    unsigned int initrd_base, unsigned int initrd_size);

#endif
