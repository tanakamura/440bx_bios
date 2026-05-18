#ifndef BIOS_RTC_H
#define BIOS_RTC_H

typedef int (*bios_rtc_enable_cmos_fn)(void);

void bios_rtc_prepare_for_linux(bios_rtc_enable_cmos_fn enable_extended_cmos);
int bios_rtc_read_time_bcd(unsigned char* hour_bcd, unsigned char* min_bcd,
                           unsigned char* sec_bcd);
int bios_rtc_read_date_bcd(unsigned char* year_bcd, unsigned char* mon_bcd,
                           unsigned char* day_bcd);
int bios_rtc_set_time_bcd(unsigned char hour_bcd, unsigned char min_bcd,
                          unsigned char sec_bcd);
int bios_rtc_set_date_bcd(unsigned char year_bcd, unsigned char mon_bcd,
                          unsigned char day_bcd);

#endif
