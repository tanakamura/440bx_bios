#ifndef LEGACY_PLATFORM_H
#define LEGACY_PLATFORM_H

#include "bios_memory.h"

#define LEGACY_BOOT_PRIORITY_AUTO 0u
#define LEGACY_BOOT_PRIORITY_IDE 1u
#define LEGACY_BOOT_PRIORITY_USB 2u

#define LEGACY_HDD_KIND_NONE 0u
#define LEGACY_HDD_KIND_IDE 1u
#define LEGACY_HDD_KIND_USB 2u

struct legacy_hdd_geometry {
    unsigned int total_sectors;
    unsigned short cylinders;
    unsigned short heads;
    unsigned short sectors_per_track;
};

struct legacy_e820_entry {
    unsigned int base_low;
    unsigned int base_high;
    unsigned int length_low;
    unsigned int length_high;
    unsigned int type;
} __attribute__((packed));
void legacy_serial_write_char(char c);
void legacy_serial_write_string(const char* s);
void legacy_serial_write_hex8(unsigned char value);
void legacy_serial_write_hex16(unsigned short value);
void legacy_serial_write_hex32(unsigned int value);
void legacy_serial_write_u32(unsigned int value);
unsigned char legacy_hdd_is_present(void);
unsigned char legacy_hdd_current_kind(void);
unsigned char legacy_hdd_select_kind(unsigned char kind);
void legacy_hdd_get_geometry(struct legacy_hdd_geometry* geometry);
int legacy_hdd_read_sectors(unsigned int lba, unsigned int count,
                            unsigned int dest);
int legacy_hdd_load_mbr_boot_sector(unsigned int dest);
int legacy_rtc_read_time_bcd(unsigned char* hour_bcd, unsigned char* min_bcd,
                             unsigned char* sec_bcd);
int legacy_rtc_read_date_bcd(unsigned char* year_bcd, unsigned char* mon_bcd,
                             unsigned char* day_bcd);
int legacy_rtc_set_time_bcd(unsigned char hour_bcd, unsigned char min_bcd,
                            unsigned char sec_bcd);
int legacy_rtc_set_date_bcd(unsigned char year_bcd, unsigned char mon_bcd,
                            unsigned char day_bcd);
unsigned int legacy_memory_extended_usable_end(unsigned int total_bytes);
unsigned int legacy_memory_e820_entry_count(unsigned int total_bytes);
int legacy_memory_e820_get_entry(unsigned int total_bytes, unsigned int index,
                                 struct legacy_e820_entry* entry);

#endif
