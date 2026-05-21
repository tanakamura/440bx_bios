#include "app/legacy/legacy_time.h"

#include "bios_pci.h"
#include "legacy_lowmem.h"
#include "app/legacy/legacy_platform.h"

#define BDA_TICK_COUNT 0x046cu
#define BDA_MIDNIGHT_FLAG 0x0470u

static void rm_set_cf(struct rm_int13_frame* f) { f->flags |= 0x0001u; }

static void rm_clear_cf(struct rm_int13_frame* f) { f->flags &= 0xfffeu; }

static void legacy_set_tick_counter(unsigned int* tick_counter,
                                    unsigned int ticks) {
    *tick_counter = ticks;
    *legacy_lowmem_u32(BDA_TICK_COUNT) = ticks;
}

static int legacy_int1a_pci_bios(struct rm_int13_frame* f) {
    unsigned char sub = (unsigned char)f->ax;
    unsigned char bus = (unsigned char)(f->bx >> 8);
    unsigned char devfn = (unsigned char)f->bx;
    unsigned char dev = (unsigned char)(devfn >> 3);
    unsigned char fn = (unsigned char)(devfn & 7u);
    unsigned char reg = (unsigned char)f->di;
    unsigned int value;
    unsigned int index;
    unsigned char last_bus = 1u;

    if (sub == 0x01u) {
        f->ax = 0x0001u;
        f->bx = 0x0210u;
        f->cx = (unsigned short)((f->cx & 0xff00u) | last_bus);
        f->edx32 = 0x20494350u;
        f->dx = 0x4350u;
        rm_clear_cf(f);
        return 0;
    }

    if (sub == 0x02u || sub == 0x03u) {
        unsigned int wanted =
            sub == 0x02u ? (((unsigned int)f->cx << 16) | f->dx)
                         : (f->ecx32 & 0x00ffffffu);
        unsigned int seen = 0u;
        unsigned char b;
        unsigned char d;
        unsigned char n;

        if (sub == 0x02u && f->dx == 0xffffu) {
            f->ax = (unsigned short)(0x8300u | sub);
            rm_set_cf(f);
            return 0;
        }

        index = f->si;
        for (b = 0u; b <= last_bus; ++b) {
            for (d = 0u; d < 32u; ++d) {
                for (n = 0u; n < 8u; ++n) {
                    unsigned int id = pci_read32(b, d, n, 0x00u);
                    if ((id & 0xffffu) == 0xffffu) {
                        continue;
                    }
                    value = sub == 0x02u ? id
                                         : (pci_read32(b, d, n, 0x08u) >> 8);
                    if (value != wanted) {
                        continue;
                    }
                    if (seen++ != index) {
                        continue;
                    }
                    f->ax = (unsigned short)(f->ax & 0x00ffu);
                    f->bx = (unsigned short)(((unsigned short)b << 8) |
                                             (unsigned short)((d << 3) | n));
                    rm_clear_cf(f);
                    return 0;
                }
            }
        }
        f->ax = (unsigned short)(0x8600u | sub);
        rm_set_cf(f);
        return 0;
    }

    if (sub >= 0x08u && sub <= 0x0du) {
        if ((reg & ((sub == 0x09u || sub == 0x0cu) ? 1u :
                    (sub == 0x0au || sub == 0x0du) ? 3u : 0u)) != 0u) {
            f->ax = (unsigned short)(0x8700u | sub);
            rm_set_cf(f);
            return 0;
        }

        if (sub == 0x08u) {
            f->cx = (unsigned short)((f->cx & 0xff00u) |
                                     pci_read8(bus, dev, fn, reg));
        } else if (sub == 0x09u) {
            f->cx = pci_read16(bus, dev, fn, reg);
        } else if (sub == 0x0au) {
            value = pci_read32(bus, dev, fn, reg);
            f->ecx32 = value;
            f->cx = (unsigned short)value;
        } else if (sub == 0x0bu) {
            pci_write8(bus, dev, fn, reg, (unsigned char)f->cx);
        } else if (sub == 0x0cu) {
            pci_write16(bus, dev, fn, reg, f->cx);
        } else {
            pci_write32(bus, dev, fn, reg, f->ecx32);
        }
        f->ax = (unsigned short)(f->ax & 0x00ffu);
        rm_clear_cf(f);
        return 0;
    }

    f->ax = (unsigned short)(0x8100u | sub);
    rm_set_cf(f);
    return 0;
}

void legacy_int1a_service(struct rm_int13_frame* f,
                          unsigned int* tick_counter) {
    if ((unsigned char)(f->ax >> 8) == 0xb1u) {
        (void)legacy_int1a_pci_bios(f);
        return;
    }
    switch ((unsigned char)(f->ax >> 8)) {
        case 0x00u:
            f->cx = (unsigned short)(*tick_counter >> 16);
            f->dx = (unsigned short)*tick_counter;
            f->ax = (unsigned short)*legacy_lowmem_u8(BDA_MIDNIGHT_FLAG);
            *legacy_lowmem_u8(BDA_MIDNIGHT_FLAG) = 0u;
            rm_clear_cf(f);
            return;
        case 0x01u:
            legacy_set_tick_counter(tick_counter,
                                    ((unsigned int)f->cx << 16) | f->dx);
            *legacy_lowmem_u8(BDA_MIDNIGHT_FLAG) = 0u;
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
