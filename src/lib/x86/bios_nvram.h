#ifndef BIOS_NVRAM_H
#define BIOS_NVRAM_H

#define BIOS_NVRAM_MAGIC 0x334e5842u
#define BIOS_NVRAM_SIZE 128u
#define BIOS_NVRAM_PARTITION_OFF 4u
#define BIOS_NVRAM_FLAGS0_OFF 5u
#define BIOS_NVRAM_BOOT_PRIORITY_OFF 6u
#define BIOS_NVRAM_CMDLINE_OFF 7u
#define BIOS_NVRAM_CMDLINE_MAX (BIOS_NVRAM_SIZE - BIOS_NVRAM_CMDLINE_OFF)

#define BIOS_NVRAM_FLAGS0_MEMTEST 0x01u
#define BIOS_NVRAM_FLAGS0_SERIAL_CONSOLE 0x02u
#define BIOS_NVRAM_FLAGS0_VESA_1024_768 0x04u
#define BIOS_NVRAM_FLAGS0_RUN_TEST_BLOB 0x08u
#define BIOS_NVRAM_FLAGS0_KNOWN_MASK 0x0fu
#define BIOS_NVRAM_FLAGS0_DEFAULT BIOS_NVRAM_FLAGS0_SERIAL_CONSOLE

#define BIOS_NVRAM_BOOT_PRIORITY_AUTO 0u
#define BIOS_NVRAM_BOOT_PRIORITY_IDE 1u
#define BIOS_NVRAM_BOOT_PRIORITY_USB 2u
#define BIOS_NVRAM_BOOT_PRIORITY_DEFAULT BIOS_NVRAM_BOOT_PRIORITY_AUTO

struct bios_nvram_settings {
    unsigned char flags0;
    unsigned char boot_priority;
    unsigned char vmlinux_partition;
    unsigned char enable_memtest;
    unsigned char run_test_blob;
    char linux_cmdline_suffix[BIOS_NVRAM_CMDLINE_MAX];
};

int bios_nvram_enable_extended_cmos(void);
void bios_nvram_init_defaults(void);
void bios_nvram_load_settings(struct bios_nvram_settings* settings);
void bios_nvram_save_partition(unsigned char part);
void bios_nvram_save_flags0(unsigned char flags0);
void bios_nvram_save_boot_priority(unsigned char priority);
void bios_nvram_save_cmdline_suffix(const char* text);

#endif
