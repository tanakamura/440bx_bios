#include "app/legacy/legacy_platform.h"

static struct legacy_platform_ops legacy_ops;

void legacy_platform_init(const struct legacy_platform_ops* ops) {
    if (ops == 0) {
        return;
    }
    legacy_ops = *ops;
}

void legacy_serial_write_char(char c) {
    if (legacy_ops.serial_write_char != 0) {
        legacy_ops.serial_write_char(c);
    }
}

void legacy_serial_write_string(const char* s) {
    if (legacy_ops.serial_write_string != 0) {
        legacy_ops.serial_write_string(s);
    }
}

void legacy_serial_write_hex8(unsigned char value) {
    if (legacy_ops.serial_write_hex8 != 0) {
        legacy_ops.serial_write_hex8(value);
    }
}

void legacy_serial_write_hex16(unsigned short value) {
    if (legacy_ops.serial_write_hex16 != 0) {
        legacy_ops.serial_write_hex16(value);
    }
}

void legacy_serial_write_hex32(unsigned int value) {
    if (legacy_ops.serial_write_hex32 != 0) {
        legacy_ops.serial_write_hex32(value);
    }
}

void legacy_serial_write_u32(unsigned int value) {
    if (legacy_ops.serial_write_u32 != 0) {
        legacy_ops.serial_write_u32(value);
    }
}

unsigned char legacy_hdd_is_present(void) {
    if (legacy_ops.hdd_is_present == 0) {
        return 0u;
    }
    return legacy_ops.hdd_is_present();
}

unsigned char legacy_hdd_current_kind(void) {
    if (legacy_ops.hdd_current_kind == 0) {
        return LEGACY_HDD_KIND_NONE;
    }
    return legacy_ops.hdd_current_kind();
}

unsigned char legacy_hdd_select_kind(unsigned char kind) {
    if (legacy_ops.hdd_select_kind == 0) {
        return 0u;
    }
    return legacy_ops.hdd_select_kind(kind);
}

void legacy_hdd_get_geometry(struct legacy_hdd_geometry* geometry) {
    if (geometry == 0) {
        return;
    }
    if (legacy_ops.hdd_get_geometry != 0) {
        legacy_ops.hdd_get_geometry(geometry);
        return;
    }
    geometry->total_sectors = 0u;
    geometry->cylinders = 0u;
    geometry->heads = 0u;
    geometry->sectors_per_track = 0u;
}

int legacy_hdd_read_sectors(unsigned int lba, unsigned int count,
                            unsigned int dest) {
    if (legacy_ops.hdd_read_sectors == 0) {
        return -1;
    }
    return legacy_ops.hdd_read_sectors(lba, count, dest);
}

int legacy_hdd_load_mbr_boot_sector(unsigned int dest) {
    if (legacy_ops.hdd_load_mbr_boot_sector == 0) {
        return -1;
    }
    return legacy_ops.hdd_load_mbr_boot_sector(dest);
}

int legacy_rtc_read_time_bcd(unsigned char* hour_bcd, unsigned char* min_bcd,
                             unsigned char* sec_bcd) {
    if (legacy_ops.rtc_read_time_bcd == 0) {
        return -1;
    }
    return legacy_ops.rtc_read_time_bcd(hour_bcd, min_bcd, sec_bcd);
}

int legacy_rtc_read_date_bcd(unsigned char* year_bcd, unsigned char* mon_bcd,
                             unsigned char* day_bcd) {
    if (legacy_ops.rtc_read_date_bcd == 0) {
        return -1;
    }
    return legacy_ops.rtc_read_date_bcd(year_bcd, mon_bcd, day_bcd);
}

int legacy_rtc_set_time_bcd(unsigned char hour_bcd, unsigned char min_bcd,
                            unsigned char sec_bcd) {
    if (legacy_ops.rtc_set_time_bcd == 0) {
        return -1;
    }
    return legacy_ops.rtc_set_time_bcd(hour_bcd, min_bcd, sec_bcd);
}

int legacy_rtc_set_date_bcd(unsigned char year_bcd, unsigned char mon_bcd,
                            unsigned char day_bcd) {
    if (legacy_ops.rtc_set_date_bcd == 0) {
        return -1;
    }
    return legacy_ops.rtc_set_date_bcd(year_bcd, mon_bcd, day_bcd);
}
