#include "bios_rtc.h"

#include "bios_io.h"
#include "bios_serial.h"

#define CMOS_SECONDS 0x00u
#define CMOS_MINUTES 0x02u
#define CMOS_HOURS 0x04u
#define CMOS_DAY 0x07u
#define CMOS_MONTH 0x08u
#define CMOS_YEAR 0x09u
#define CMOS_STATUS_A 0x0au
#define CMOS_STATUS_B 0x0bu
#define CMOS_STATUS_C 0x0cu
#define CMOS_STATUS_D 0x0du
#define RTC_REGA_32KHZ_RATE6 0x26u
#define RTC_REGB_SET 0x80u
#define RTC_REGB_24H_BCD 0x02u

static unsigned char cmos_read(unsigned char index) {
    outb(0x0070u, (unsigned char)(index | 0x80u));
    return inb(0x0071u);
}

static void cmos_write(unsigned char index, unsigned char value) {
    outb(0x0070u, (unsigned char)(index | 0x80u));
    outb(0x0071u, value);
}

static unsigned char bin_to_bcd(unsigned char value) {
    return (unsigned char)(((value / 10u) << 4) | (value % 10u));
}

static unsigned char bcd_to_bin(unsigned char value) {
    return (unsigned char)(((value >> 4) * 10u) + (value & 0x0fu));
}

static unsigned char rtc_decode(unsigned char value, unsigned char status_b) {
    if ((status_b & 0x04u) != 0u) {
        return value;
    }
    if ((value & 0x0fu) > 9u || ((value >> 4) & 0x0fu) > 9u) {
        return 0xffu;
    }
    return bcd_to_bin(value);
}

static unsigned char rtc_decode_hour(unsigned char hour,
                                     unsigned char status_b) {
    unsigned char hour_bin =
        rtc_decode((unsigned char)(hour & 0x7fu), status_b);
    if (hour_bin == 0xffu) {
        return 0xffu;
    }
    if ((status_b & 0x02u) == 0u) {
        unsigned char pm = (unsigned char)(hour & 0x80u);
        if (hour_bin == 12u) {
            hour_bin = 0u;
        }
        if (pm != 0u && hour_bin < 12u) {
            hour_bin = (unsigned char)(hour_bin + 12u);
        }
    }
    return hour_bin;
}

static int rtc_raw_datetime_valid(unsigned char status_b, unsigned char sec,
                                  unsigned char min, unsigned char hour,
                                  unsigned char day, unsigned char mon,
                                  unsigned char year) {
    unsigned char sec_bin = rtc_decode(sec, status_b);
    unsigned char min_bin = rtc_decode(min, status_b);
    unsigned char hour_bin = rtc_decode_hour(hour, status_b);
    unsigned char day_bin = rtc_decode(day, status_b);
    unsigned char mon_bin = rtc_decode(mon, status_b);
    unsigned char year_bin = rtc_decode(year, status_b);

    return sec_bin <= 59u && min_bin <= 59u && hour_bin <= 23u &&
           day_bin >= 1u && day_bin <= 31u && mon_bin >= 1u && mon_bin <= 12u &&
           year_bin <= 99u;
}

static void rtc_write_datetime_bcd(unsigned char sec, unsigned char min,
                                   unsigned char hour, unsigned char day,
                                   unsigned char mon, unsigned char year) {
    cmos_write(CMOS_SECONDS, sec);
    cmos_write(CMOS_MINUTES, min);
    cmos_write(CMOS_HOURS, hour);
    cmos_write(CMOS_DAY, day);
    cmos_write(CMOS_MONTH, mon);
    cmos_write(CMOS_YEAR, year);
}

void bios_rtc_prepare_for_linux(bios_rtc_enable_cmos_fn enable_extended_cmos) {
    unsigned char status_b;
    unsigned char sec;
    unsigned char min;
    unsigned char hour;
    unsigned char day;
    unsigned char mon;
    unsigned char year;
    unsigned char valid;

    if (enable_extended_cmos != 0) {
        (void)enable_extended_cmos();
    }

    status_b = cmos_read(CMOS_STATUS_B);
    cmos_write(CMOS_STATUS_B, (unsigned char)(status_b | RTC_REGB_SET));
    sec = cmos_read(CMOS_SECONDS);
    min = cmos_read(CMOS_MINUTES);
    hour = cmos_read(CMOS_HOURS);
    day = cmos_read(CMOS_DAY);
    mon = cmos_read(CMOS_MONTH);
    year = cmos_read(CMOS_YEAR);
    valid = (unsigned char)rtc_raw_datetime_valid(status_b, sec, min, hour, day,
                                                  mon, year);

    cmos_write(CMOS_STATUS_A, RTC_REGA_32KHZ_RATE6);
    if (valid != 0u) {
        rtc_write_datetime_bcd(bin_to_bcd(rtc_decode(sec, status_b)),
                               bin_to_bcd(rtc_decode(min, status_b)),
                               bin_to_bcd(rtc_decode_hour(hour, status_b)),
                               bin_to_bcd(rtc_decode(day, status_b)),
                               bin_to_bcd(rtc_decode(mon, status_b)),
                               bin_to_bcd(rtc_decode(year, status_b)));
    } else {
        rtc_write_datetime_bcd(0x00u, 0x00u, 0x00u, 0x01u, 0x01u, 0x26u);
    }
    cmos_write(CMOS_STATUS_B, RTC_REGB_24H_BCD);
    (void)cmos_read(CMOS_STATUS_C);
    (void)cmos_read(CMOS_STATUS_D);

    serial_write_string("RTC Linux sane ");
    serial_write_string(valid != 0u ? "keep\r\n" : "default\r\n");
}

static unsigned char rtc_force_sane_mode(void) {
    unsigned char status_b = cmos_read(CMOS_STATUS_B);
    unsigned char sane_status_b = (unsigned char)((status_b & 0x79u) | 0x02u);
    if (sane_status_b != status_b) {
        cmos_write(CMOS_STATUS_B, sane_status_b);
    }
    return sane_status_b;
}

static unsigned char rtc_begin_set(void) {
    unsigned char sane_status_b = rtc_force_sane_mode();
    cmos_write(CMOS_STATUS_B, (unsigned char)(sane_status_b | 0x80u));
    return sane_status_b;
}

static void rtc_end_set(unsigned char sane_status_b) {
    cmos_write(CMOS_STATUS_B, sane_status_b);
}

static int rtc_wait_ready(void) {
    unsigned int timeout = 100000u;
    while (timeout-- != 0u) {
        if ((cmos_read(CMOS_STATUS_A) & 0x80u) == 0) {
            return 0;
        }
    }
    return -1;
}

int bios_rtc_read_time_bcd(unsigned char* hour_bcd, unsigned char* min_bcd,
                           unsigned char* sec_bcd) {
    unsigned char status_b;
    unsigned char hour;
    unsigned char min;
    unsigned char sec;
    unsigned char hour_bin;
    unsigned char min_bin;
    unsigned char sec_bin;

    if (rtc_wait_ready() != 0) {
        return -1;
    }
    status_b = rtc_force_sane_mode();
    sec = cmos_read(CMOS_SECONDS);
    min = cmos_read(CMOS_MINUTES);
    hour = cmos_read(CMOS_HOURS);

    if ((status_b & 0x04u) != 0) {
        sec_bin = sec;
        min_bin = min;
        hour_bin = (unsigned char)(hour & 0x7fu);
    } else {
        sec_bin = bcd_to_bin(sec);
        min_bin = bcd_to_bin(min);
        hour_bin = bcd_to_bin((unsigned char)(hour & 0x7fu));
    }

    if ((status_b & 0x02u) == 0) {
        unsigned char pm = (unsigned char)(hour & 0x80u);
        if (hour_bin == 12u) {
            hour_bin = 0u;
        }
        if (pm != 0u) {
            hour_bin = (unsigned char)(hour_bin + 12u);
        }
    }

    if (sec_bin > 59u || min_bin > 59u || hour_bin > 23u) {
        return -1;
    }

    *sec_bcd = bin_to_bcd(sec_bin);
    *min_bcd = bin_to_bcd(min_bin);
    *hour_bcd = bin_to_bcd(hour_bin);
    return 0;
}

int bios_rtc_read_date_bcd(unsigned char* year_bcd, unsigned char* mon_bcd,
                           unsigned char* day_bcd) {
    unsigned char status_b;
    unsigned char day;
    unsigned char mon;
    unsigned char year;
    unsigned char day_bin;
    unsigned char mon_bin;
    unsigned char year_bin;

    if (rtc_wait_ready() != 0) {
        return -1;
    }
    status_b = rtc_force_sane_mode();
    day = cmos_read(CMOS_DAY);
    mon = cmos_read(CMOS_MONTH);
    year = cmos_read(CMOS_YEAR);

    if ((status_b & 0x04u) != 0) {
        day_bin = day;
        mon_bin = mon;
        year_bin = year;
    } else {
        day_bin = bcd_to_bin(day);
        mon_bin = bcd_to_bin(mon);
        year_bin = bcd_to_bin(year);
    }

    if (day_bin < 1u || day_bin > 31u || mon_bin < 1u || mon_bin > 12u) {
        return -1;
    }

    *day_bcd = bin_to_bcd(day_bin);
    *mon_bcd = bin_to_bcd(mon_bin);
    *year_bcd = bin_to_bcd(year_bin);
    return 0;
}

int bios_rtc_set_time_bcd(unsigned char hour_bcd, unsigned char min_bcd,
                          unsigned char sec_bcd) {
    unsigned char sane_status_b;
    unsigned char hour_bin = bcd_to_bin((unsigned char)(hour_bcd & 0x7fu));
    unsigned char min_bin = bcd_to_bin(min_bcd);
    unsigned char sec_bin = bcd_to_bin(sec_bcd);

    if (hour_bin > 23u || min_bin > 59u || sec_bin > 59u) {
        return -1;
    }

    if (rtc_wait_ready() != 0) {
        return -1;
    }

    sane_status_b = rtc_begin_set();
    cmos_write(CMOS_SECONDS, sec_bcd);
    cmos_write(CMOS_MINUTES, min_bcd);
    cmos_write(CMOS_HOURS, (unsigned char)(hour_bcd & 0x7fu));
    rtc_end_set(sane_status_b);
    return 0;
}

int bios_rtc_set_date_bcd(unsigned char year_bcd, unsigned char mon_bcd,
                          unsigned char day_bcd) {
    unsigned char mon_bin = bcd_to_bin(mon_bcd);
    unsigned char day_bin = bcd_to_bin(day_bcd);
    unsigned char sane_status_b;

    if (mon_bin < 1u || mon_bin > 12u || day_bin < 1u || day_bin > 31u) {
        return -1;
    }

    if (rtc_wait_ready() != 0) {
        return -1;
    }

    sane_status_b = rtc_begin_set();
    cmos_write(CMOS_DAY, day_bcd);
    cmos_write(CMOS_MONTH, mon_bcd);
    cmos_write(CMOS_YEAR, year_bcd);
    rtc_end_set(sane_status_b);
    return 0;
}
