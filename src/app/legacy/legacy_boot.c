#include "app/legacy/legacy_boot.h"

#include "app/legacy/legacy_floppy.h"
#include "app/legacy/legacy_platform.h"

#define BOOT_SECTOR_LINEAR 0x00007c00u

static int prepare_boot_sector_test_floppy(unsigned char* boot_drive) {
    if (!legacy_floppy_present() ||
        legacy_floppy_read_sectors(0u, 1u, BOOT_SECTOR_LINEAR) != 0 ||
        *(volatile unsigned short*)(BOOT_SECTOR_LINEAR + 510u) != 0xaa55u) {
        return -1;
    }
    *boot_drive = 0x00u;
    legacy_serial_write_string("Booting test floppy\r\n");
    return 0;
}

static int prepare_boot_sector_current(
    unsigned char* boot_drive, legacy_boot_record_success_fn record_success) {
    if (legacy_hdd_load_mbr_boot_sector(BOOT_SECTOR_LINEAR) == 0) {
        *boot_drive = 0x80u;
        if (record_success != 0) {
            record_success(legacy_hdd_current_kind());
        }
        return 0;
    }
    return -1;
}

unsigned char legacy_prepare_boot_sector(
    unsigned char boot_priority, legacy_boot_record_success_fn record_success) {
    unsigned char boot_drive = 0x80u;

    if (prepare_boot_sector_test_floppy(&boot_drive) == 0) {
        return boot_drive;
    }
    if (boot_priority == LEGACY_BOOT_PRIORITY_IDE) {
        if (legacy_hdd_select_kind(LEGACY_HDD_KIND_IDE) != 0u &&
            prepare_boot_sector_current(&boot_drive, record_success) == 0) {
            return boot_drive;
        }
    } else if (boot_priority == LEGACY_BOOT_PRIORITY_USB) {
        if (legacy_hdd_select_kind(LEGACY_HDD_KIND_USB) != 0u &&
            prepare_boot_sector_current(&boot_drive, record_success) == 0) {
            return boot_drive;
        }
    } else {
        if (legacy_hdd_select_kind(LEGACY_HDD_KIND_IDE) != 0u &&
            prepare_boot_sector_current(&boot_drive, record_success) == 0) {
            return boot_drive;
        }
        if (legacy_hdd_select_kind(LEGACY_HDD_KIND_USB) != 0u &&
            prepare_boot_sector_current(&boot_drive, record_success) == 0) {
            return boot_drive;
        }
    }
    legacy_serial_write_string("No bootable HDD MBR\r\n");
    for (;;) {
        __asm__ volatile("hlt");
    }
}
