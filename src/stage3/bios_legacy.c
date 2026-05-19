#include "bios_legacy.h"

#include "bios_memory.h"
#include "bios_rtc.h"
#include "bios_serial.h"
#include "bios_storage.h"
#include "app/legacy/legacy_boot.h"
#include "app/legacy/legacy_floppy.h"
#include "app/legacy/legacy_platform.h"
#include "app/legacy/legacy_runtime.h"
#include "app/legacy/legacy_thunk.h"

static const unsigned short bios_ebda_segment = 0x0000u;
static const unsigned short bios_dos_base_mem_kb = 640u;

static void legacy_hdd_get_geometry_cb(struct legacy_hdd_geometry* geometry) {
    struct bios_hdd_geometry bios_geometry;

    bios_hdd_get_geometry(&bios_geometry);
    geometry->total_sectors = bios_geometry.total_sectors;
    geometry->cylinders = bios_geometry.cylinders;
    geometry->heads = bios_geometry.heads;
    geometry->sectors_per_track = bios_geometry.sectors_per_track;
}

static unsigned int
legacy_memory_extended_usable_end_cb(unsigned int total_bytes) {
    return bios_memory_extended_usable_end(total_bytes);
}

static unsigned int legacy_memory_e820_entry_count_cb(unsigned int total_bytes) {
    return bios_memory_e820_entry_count(total_bytes);
}

static int legacy_memory_e820_get_entry_cb(unsigned int total_bytes,
                                           unsigned int index,
                                           struct legacy_e820_entry* entry) {
    struct e820_entry bios_entry;

    if (bios_memory_e820_get_entry(total_bytes, index, &bios_entry) != 0) {
        return -1;
    }
    entry->base_low = bios_entry.base_low;
    entry->base_high = bios_entry.base_high;
    entry->length_low = bios_entry.length_low;
    entry->length_high = bios_entry.length_high;
    entry->type = bios_entry.type;
    return 0;
}

void bios_legacy_install_platform_ops(void) {
    struct legacy_platform_ops ops = {0};

    ops.serial_write_char = serial_write_char;
    ops.serial_write_string = serial_write_string;
    ops.serial_write_hex8 = serial_write_hex8;
    ops.serial_write_hex16 = serial_write_hex16;
    ops.serial_write_hex32 = serial_write_hex32;
    ops.serial_write_u32 = serial_write_u32;
    ops.hdd_is_present = bios_hdd_is_present;
    ops.hdd_current_kind = bios_hdd_current_kind;
    ops.hdd_select_kind = bios_hdd_select_kind;
    ops.hdd_get_geometry = legacy_hdd_get_geometry_cb;
    ops.hdd_read_sectors = bios_hdd_read_sectors;
    ops.hdd_load_mbr_boot_sector = bios_hdd_load_mbr_boot_sector;
    ops.rtc_read_time_bcd = bios_rtc_read_time_bcd;
    ops.rtc_read_date_bcd = bios_rtc_read_date_bcd;
    ops.rtc_set_time_bcd = bios_rtc_set_time_bcd;
    ops.rtc_set_date_bcd = bios_rtc_set_date_bcd;
    ops.memory_extended_usable_end = legacy_memory_extended_usable_end_cb;
    ops.memory_e820_entry_count = legacy_memory_e820_entry_count_cb;
    ops.memory_e820_get_entry = legacy_memory_e820_get_entry_cb;
    legacy_platform_init(&ops);
}

void bios_legacy_install_runtime(
    unsigned int total_bytes, unsigned char boot_priority,
    bios_legacy_record_boot_success_fn record_boot_success,
    bios_legacy_void_fn boot_pm32, bios_legacy_void_fn install_shadow) {
    struct legacy_runtime_config config;

    config.total_bytes = total_bytes;
    config.floppy_present = legacy_floppy_present();
    config.hdd_present = bios_hdd_is_present();
    config.base_mem_kb = bios_dos_base_mem_kb;
    config.ebda_segment = bios_ebda_segment;
    config.boot_priority = boot_priority;
    config.record_boot_success = record_boot_success;
    config.boot_pm32 = boot_pm32;
    config.install_shadow = install_shadow;
    legacy_runtime_init(&config);
}

unsigned char bios_legacy_prepare_boot_sector(
    unsigned char boot_priority,
    bios_legacy_record_boot_success_fn record_boot_success) {
    return legacy_prepare_boot_sector(boot_priority, record_boot_success);
}

void bios_legacy_install_boot_drive(unsigned char boot_drive) {
    legacy_install_boot_drive(boot_drive);
}

void bios_legacy_install_pm_stack_top(unsigned int pm_stack_top) {
    legacy_install_pm_stack_top(pm_stack_top);
}
