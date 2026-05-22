#include "app/legacy/legacy_platform.h"

#include "bios_rtc.h"
#include "bios_serial.h"
#include "bios_storage.h"

void legacy_serial_write_char(char c) { serial_write_char(c); }

void legacy_serial_write_string(const char* s) { serial_write_string(s); }

void legacy_serial_write_hex8(unsigned char value) { serial_write_hex8(value); }

void legacy_serial_write_hex16(unsigned short value) {
    serial_write_hex16(value);
}

void legacy_serial_write_hex32(unsigned int value) { serial_write_hex32(value); }

void legacy_serial_write_u32(unsigned int value) { serial_write_u32(value); }

unsigned char legacy_hdd_is_present(void) { return bios_hdd_is_present(); }

unsigned char legacy_hdd_current_kind(void) { return bios_hdd_current_kind(); }

unsigned char legacy_hdd_select_kind(unsigned char kind) {
    return bios_hdd_select_kind(kind);
}

void legacy_hdd_get_geometry(struct legacy_hdd_geometry* geometry) {
    struct bios_hdd_geometry bios_geometry;

    if (geometry == 0) {
        return;
    }
    bios_hdd_get_geometry(&bios_geometry);
    geometry->total_sectors = bios_geometry.total_sectors;
    geometry->cylinders = bios_geometry.cylinders;
    geometry->heads = bios_geometry.heads;
    geometry->sectors_per_track = bios_geometry.sectors_per_track;
}

int legacy_hdd_read_sectors(unsigned int lba, unsigned int count,
                            unsigned int dest) {
    return bios_hdd_read_sectors(lba, count, dest);
}

int legacy_hdd_load_mbr_boot_sector(unsigned int dest) {
    return bios_hdd_load_mbr_boot_sector(dest);
}

int legacy_rtc_read_time_bcd(unsigned char* hour_bcd, unsigned char* min_bcd,
                             unsigned char* sec_bcd) {
    return bios_rtc_read_time_bcd(hour_bcd, min_bcd, sec_bcd);
}

int legacy_rtc_read_date_bcd(unsigned char* year_bcd, unsigned char* mon_bcd,
                             unsigned char* day_bcd) {
    return bios_rtc_read_date_bcd(year_bcd, mon_bcd, day_bcd);
}

int legacy_rtc_set_time_bcd(unsigned char hour_bcd, unsigned char min_bcd,
                            unsigned char sec_bcd) {
    return bios_rtc_set_time_bcd(hour_bcd, min_bcd, sec_bcd);
}

int legacy_rtc_set_date_bcd(unsigned char year_bcd, unsigned char mon_bcd,
                            unsigned char day_bcd) {
    return bios_rtc_set_date_bcd(year_bcd, mon_bcd, day_bcd);
}

unsigned int legacy_memory_extended_usable_end(unsigned int total_bytes) {
    return bios_memory_extended_usable_end(total_bytes);
}

unsigned int legacy_memory_e820_entry_count(unsigned int total_bytes) {
    return bios_memory_e820_entry_count(total_bytes);
}

int legacy_memory_e820_get_entry(unsigned int total_bytes, unsigned int index,
                                 struct legacy_e820_entry* entry) {
    struct e820_entry bios_entry;

    if (entry == 0) {
        return -1;
    }
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
