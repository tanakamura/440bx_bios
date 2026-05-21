#ifndef LINUX_LOADER_ABI_H
#define LINUX_LOADER_ABI_H

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

typedef void (*linux_loader_void_fn)(void);
typedef void (*linux_loader_record_boot_success_fn)(unsigned char kind);
typedef unsigned char* (*linux_loader_buffer_fn)(void);
typedef unsigned int (*linux_loader_vbe_fn)(unsigned int mode);

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
    unsigned int total_bytes;
    unsigned int acpi_rsdp_linear;
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
    void (*serial_write_string)(const char* s);
    void (*serial_write_hex8)(unsigned char value);
    void (*serial_write_hex16)(unsigned short value);
    void (*serial_write_hex32)(unsigned int value);
    void (*serial_write_u32)(unsigned int value);
    unsigned char (*hdd_is_present)(void);
    unsigned char (*hdd_current_kind)(void);
    unsigned char (*hdd_select_kind)(unsigned char kind);
    void (*hdd_get_geometry)(struct linux_loader_hdd_geometry* geometry);
    int (*hdd_read_sectors)(unsigned int lba, unsigned int count,
                            unsigned int dest);
    unsigned int (*memory_extended_usable_end)(unsigned int total_bytes);
    unsigned int (*memory_e820_entry_count)(unsigned int total_bytes);
    int (*memory_e820_get_entry)(unsigned int total_bytes, unsigned int index,
                                 struct linux_loader_e820_entry* entry);
    unsigned int runtime_protect_base;
    unsigned int runtime_protect_size;
    void (*release_boot_services)(void);
};

#endif
