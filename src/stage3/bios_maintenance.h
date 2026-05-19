#ifndef BIOS_MAINTENANCE_H
#define BIOS_MAINTENANCE_H

struct bios_maintenance_config {
    unsigned char* flags0;
    unsigned char* boot_priority;
    unsigned char* vmlinux_partition;
    unsigned char* enable_memtest;
    unsigned char* run_test_blob;
    char* linux_cmdline_suffix;
    void (*save_partition)(unsigned char part);
    void (*save_flags0)(unsigned char flags0);
    void (*save_boot_priority)(unsigned char priority);
    void (*save_cmdline_suffix)(const char* text);
    void (*reset_defaults)(void);
};

void bios_maintenance_prompt(const struct bios_maintenance_config* config);

#endif
