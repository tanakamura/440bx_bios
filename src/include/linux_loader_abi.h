#ifndef LINUX_LOADER_ABI_H
#define LINUX_LOADER_ABI_H

#include "app_platform_abi.h"

#define LINUX_LOADER_BOOT_PARAMS 0x00090000u
#define LINUX_LOADER_RSDP_LINEAR 0x0009fc00u
#define LINUX_LOADER_TEST_ELF_IMAGE_LINEAR 0x00280000u
#define LINUX_LOADER_TEST_ELF_IMAGE_CAPACITY 0x00040000u

#define LINUX_LOADER_BOOT_PRIORITY_AUTO 0u
#define LINUX_LOADER_BOOT_PRIORITY_IDE 1u
#define LINUX_LOADER_BOOT_PRIORITY_USB 2u

#define LINUX_LOADER_HDD_KIND_NONE 0u
#define LINUX_LOADER_HDD_KIND_IDE 1u
#define LINUX_LOADER_HDD_KIND_USB 2u

struct linux_loader_hdd_geometry {
    unsigned int total_sectors;
    unsigned short cylinders;
    unsigned short heads;
    unsigned short sectors_per_track;
};

struct linux_loader_e820_entry {
    unsigned int base_low;
    unsigned int base_high;
    unsigned int length_low;
    unsigned int length_high;
    unsigned int type;
} __attribute__((packed));

struct linux_loader_config {
    struct app_platform_info platform;
    unsigned int runtime_protect_base;
    unsigned int runtime_protect_size;
};

#endif
