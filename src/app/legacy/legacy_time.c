#include "app/legacy/legacy_time.h"

#include "app/legacy/legacy_platform.h"

#define BDA_TICK_COUNT 0x046cu
#define BDA_MIDNIGHT_FLAG 0x0470u

static void rm_set_cf(struct rm_int13_frame* f) { f->flags |= 0x0001u; }

static void rm_clear_cf(struct rm_int13_frame* f) { f->flags &= 0xfffeu; }

static void legacy_set_tick_counter(unsigned int* tick_counter,
                                    unsigned int ticks) {
    *tick_counter = ticks;
    *(volatile unsigned int*)BDA_TICK_COUNT = ticks;
}

void legacy_int1a_service(struct rm_int13_frame* f,
                          unsigned int* tick_counter) {
    switch ((unsigned char)(f->ax >> 8)) {
        case 0x00u:
            f->cx = (unsigned short)(*tick_counter >> 16);
            f->dx = (unsigned short)*tick_counter;
            f->ax = (unsigned short)(*(volatile unsigned char*)
                                         BDA_MIDNIGHT_FLAG);
            *(volatile unsigned char*)BDA_MIDNIGHT_FLAG = 0u;
            rm_clear_cf(f);
            return;
        case 0x01u:
            legacy_set_tick_counter(tick_counter,
                                    ((unsigned int)f->cx << 16) | f->dx);
            *(volatile unsigned char*)BDA_MIDNIGHT_FLAG = 0u;
            rm_clear_cf(f);
            return;
        case 0x02u: {
            unsigned char hour_bcd;
            unsigned char min_bcd;
            unsigned char sec_bcd;
            if (legacy_rtc_read_time_bcd(&hour_bcd, &min_bcd, &sec_bcd) != 0) {
                hour_bcd = 0x12u;
                min_bcd = 0x00u;
                sec_bcd = 0x00u;
            }
            f->cx = (unsigned short)(((unsigned short)hour_bcd << 8) |
                                     min_bcd);
            f->dx = (unsigned short)(((unsigned short)sec_bcd << 8) | 0x00u);
            rm_clear_cf(f);
            return;
        }
        case 0x04u: {
            unsigned char day_bcd;
            unsigned char mon_bcd;
            unsigned char year_bcd;
            unsigned char century = 0x20u;
            if (legacy_rtc_read_date_bcd(&year_bcd, &mon_bcd, &day_bcd) != 0) {
                year_bcd = 0x26u;
                mon_bcd = 0x05u;
                day_bcd = 0x08u;
            }
            f->cx = (unsigned short)(((unsigned short)century << 8) |
                                     year_bcd);
            f->dx = (unsigned short)(((unsigned short)mon_bcd << 8) |
                                     day_bcd);
            rm_clear_cf(f);
            return;
        }
        case 0x03u:
            if (legacy_rtc_set_time_bcd((unsigned char)(f->cx >> 8),
                                        (unsigned char)f->cx,
                                        (unsigned char)(f->dx >> 8)) != 0) {
                f->ax = (unsigned short)((0x86u << 8) | (f->ax & 0x00ffu));
                rm_set_cf(f);
            } else {
                rm_clear_cf(f);
            }
            return;
        case 0x05u:
            if (legacy_rtc_set_date_bcd((unsigned char)f->cx,
                                        (unsigned char)(f->dx >> 8),
                                        (unsigned char)f->dx) != 0) {
                f->ax = (unsigned short)((0x86u << 8) | (f->ax & 0x00ffu));
                rm_set_cf(f);
            } else {
                rm_clear_cf(f);
            }
            return;
        default:
            f->ax = (unsigned short)((0x86u << 8) | (f->ax & 0x00ffu));
            rm_set_cf(f);
            return;
    }
}
