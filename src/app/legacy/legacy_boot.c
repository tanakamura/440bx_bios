#include "app/legacy/legacy_boot.h"

#include "app/legacy/legacy_floppy.h"
#include "app/legacy/legacy_platform.h"
#include "app/legacy/legacy_thunk.h"
#include "app_shadow.h"
#include "bios_nvram.h"

#define BOOT_SECTOR_LINEAR 0x00007c00u

extern void bios_call_vgabios_init_pm32(unsigned int bdf);

static struct app_platform_info legacy_boot_platform;

void legacy_boot_init(const struct app_platform_info* platform) {
    legacy_boot_platform = (struct app_platform_info){0};
    if (platform != 0) {
        legacy_boot_platform = *platform;
    }
}

static int prepare_boot_sector_test_floppy(unsigned char* boot_drive) {
    if (!legacy_floppy_present() ||
        legacy_floppy_read_sectors(0u, 1u, BOOT_SECTOR_LINEAR) != 0 ||
        *(volatile unsigned short*)(BOOT_SECTOR_LINEAR + 510u) != 0xaa55u) {
        return -1;
    }
    *boot_drive = 0x00u;
    legacy_install_boot_drive(*boot_drive);
    legacy_serial_write_string("Booting test floppy\r\n");
    return 0;
}

static int prepare_boot_sector_current(
    unsigned char* boot_drive, unsigned char boot_priority) {
    if (legacy_hdd_load_mbr_boot_sector(BOOT_SECTOR_LINEAR) == 0) {
        unsigned char kind = legacy_hdd_current_kind();
        *boot_drive = 0x80u;
        legacy_install_boot_drive(*boot_drive);
        if (boot_priority == BIOS_NVRAM_BOOT_PRIORITY_AUTO &&
            (kind == BIOS_NVRAM_BOOT_PRIORITY_IDE ||
             kind == BIOS_NVRAM_BOOT_PRIORITY_USB)) {
            bios_nvram_save_boot_priority(kind);
        }
        return 0;
    }
    return -1;
}

unsigned char legacy_prepare_boot_sector(void) {
    unsigned char boot_drive = 0x80u;
    unsigned char boot_priority = legacy_boot_platform.nvram.boot_priority;

    app_shadow_install(legacy_boot_platform.total_bytes);
    app_shadow_install_vgabios(legacy_boot_platform.total_bytes);
    app_shadow_init_vgabios(bios_call_vgabios_init_pm32);

    if (prepare_boot_sector_test_floppy(&boot_drive) == 0) {
        return boot_drive;
    }
    if (boot_priority == LEGACY_BOOT_PRIORITY_IDE) {
        if (legacy_hdd_select_kind(LEGACY_HDD_KIND_IDE) != 0u &&
            prepare_boot_sector_current(&boot_drive, boot_priority) == 0) {
            return boot_drive;
        }
    } else if (boot_priority == LEGACY_BOOT_PRIORITY_USB) {
        if (legacy_hdd_select_kind(LEGACY_HDD_KIND_USB) != 0u &&
            prepare_boot_sector_current(&boot_drive, boot_priority) == 0) {
            return boot_drive;
        }
    } else {
        if (legacy_hdd_select_kind(LEGACY_HDD_KIND_IDE) != 0u &&
            prepare_boot_sector_current(&boot_drive, boot_priority) == 0) {
            return boot_drive;
        }
        if (legacy_hdd_select_kind(LEGACY_HDD_KIND_USB) != 0u &&
            prepare_boot_sector_current(&boot_drive, boot_priority) == 0) {
            return boot_drive;
        }
    }
    legacy_serial_write_string("No bootable HDD MBR\r\n");
    for (;;) {
        __asm__ volatile("hlt");
    }
}
