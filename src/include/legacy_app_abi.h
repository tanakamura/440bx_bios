#ifndef LEGACY_APP_ABI_H
#define LEGACY_APP_ABI_H

#define LEGACY_APP_EXPORTS_MAGIC 0x4c415058u
#define LEGACY_APP_EXPORTS_VERSION 1u

typedef void (*legacy_app_record_success_fn)(unsigned char kind);
typedef void (*legacy_app_pm32_fn)(void);
typedef void (*legacy_app_void_fn)(void);

struct legacy_app_exports {
    unsigned int magic;
    unsigned int version;
    unsigned int size;
    unsigned char (*prepare_boot_sector)(
        unsigned char boot_priority,
        legacy_app_record_success_fn record_success);
    void (*install_boot_drive)(unsigned char boot_drive);
    void (*install_pm_stack_top)(unsigned int stack_top);
};

struct legacy_runtime_config {
    unsigned int total_bytes;
    unsigned short base_mem_kb;
    unsigned short ebda_segment;
    unsigned char boot_priority;
    legacy_app_record_success_fn record_boot_success;
    legacy_app_pm32_fn boot_pm32;
    legacy_app_void_fn install_shadow;
    struct legacy_app_exports* exports;
};

#endif
