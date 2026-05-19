#include "legacy_floppy.h"
#include "legacy_int13.h"
#include "legacy_platform.h"

struct rm_dap {
    unsigned char size;
    unsigned char reserved;
    unsigned short count;
    unsigned short off;
    unsigned short seg;
    unsigned int lba_low;
    unsigned int lba_high;
} __attribute__((packed));

struct rm_edd_params {
    unsigned short size;
    unsigned short information;
    unsigned int cylinders;
    unsigned int heads;
    unsigned int sectors;
    unsigned int total_sectors_low;
    unsigned int total_sectors_high;
    unsigned short bytes_per_sector;
    unsigned int edd_config_params;
} __attribute__((packed));

static unsigned int rm_seg_off_to_linear(unsigned short seg,
                                         unsigned short off) {
    return ((unsigned int)seg << 4) + off;
}

static void rm_set_cf(struct rm_int13_frame* f) { f->flags |= 0x0001u; }

static void rm_clear_cf(struct rm_int13_frame* f) { f->flags &= 0xfffeu; }

static void legacy_memset(void* dst, unsigned char value, unsigned int len) {
    unsigned char* p = (unsigned char*)dst;
    while (len-- != 0u) {
        *p++ = value;
    }
}

static void floppy_params(struct rm_int13_frame* f,
                          unsigned int floppy_dpt_linear) {
    unsigned int sectors = legacy_floppy_sector_count();
    unsigned int max_cyl =
        (sectors / (LEGACY_FLOPPY_HEADS * LEGACY_FLOPPY_SPT)) - 1u;

    if (max_cyl > 79u) {
        max_cyl = 79u;
    }
    f->ax = 0u;
    f->bx = (unsigned short)((f->bx & 0xff00u) | 0x04u);
    f->cx = (unsigned short)(((max_cyl & 0xffu) << 8) | LEGACY_FLOPPY_SPT |
                             ((max_cyl >> 2) & 0xc0u));
    f->dx = (unsigned short)(((LEGACY_FLOPPY_HEADS - 1u) << 8) | 0x01u);
    f->es = (unsigned short)(floppy_dpt_linear >> 4);
    f->di = (unsigned short)(floppy_dpt_linear & 0x0fu);
    rm_clear_cf(f);
}

static void floppy_chs_read(struct rm_int13_frame* f) {
    unsigned int count = (unsigned char)f->ax;
    unsigned int cylinder =
        ((unsigned int)(f->cx >> 8) | (((unsigned int)f->cx & 0x00c0u) << 2));
    unsigned int sector = (unsigned int)(f->cx & 0x003fu);
    unsigned int head = (unsigned int)(f->dx >> 8);
    unsigned int lba;
    unsigned int dest;

    if (sector == 0u || sector > LEGACY_FLOPPY_SPT || count == 0u ||
        head >= LEGACY_FLOPPY_HEADS) {
        f->ax = 0x0100u;
        rm_set_cf(f);
        return;
    }

    lba = ((cylinder * LEGACY_FLOPPY_HEADS) + head) * LEGACY_FLOPPY_SPT +
          sector - 1u;
    dest = rm_seg_off_to_linear(f->es, f->bx);
    if (legacy_floppy_read_sectors(lba, count, dest) != 0) {
        f->ax = 0x0100u;
        rm_set_cf(f);
        return;
    }
    f->ax = (unsigned short)count;
    rm_clear_cf(f);
}

static void floppy_service(struct rm_int13_frame* f,
                           unsigned int floppy_dpt_linear) {
    unsigned char ah = (unsigned char)(f->ax >> 8);

    if (!legacy_floppy_present() || (unsigned char)f->dx != 0u) {
        f->ax = 0x0100u;
        rm_set_cf(f);
        return;
    }

    switch (ah) {
        case 0x00u:
            f->ax &= 0x00ffu;
            rm_clear_cf(f);
            return;
        case 0x02u:
            floppy_chs_read(f);
            return;
        case 0x03u:
            f->ax = 0x0300u;
            rm_set_cf(f);
            return;
        case 0x04u:
        case 0x16u:
            f->ax &= 0x00ffu;
            rm_clear_cf(f);
            return;
        case 0x08u:
            floppy_params(f, floppy_dpt_linear);
            return;
        case 0x15u:
            f->ax = (unsigned short)((0x02u << 8) | (f->ax & 0x00ffu));
            rm_clear_cf(f);
            return;
        default:
            f->ax = 0x0100u;
            rm_set_cf(f);
            return;
    }
}

static void hdd_params(struct rm_int13_frame* f) {
    struct legacy_hdd_geometry geometry;
    unsigned int max_cyl;
    unsigned int max_head;
    unsigned int spt;

    legacy_hdd_get_geometry(&geometry);
    max_cyl = geometry.cylinders - 1u;
    max_head = geometry.heads - 1u;
    spt = geometry.sectors_per_track;

    f->ax = 0u;
    f->cx = (unsigned short)(((max_cyl & 0xffu) << 8) | spt |
                             ((max_cyl >> 2) & 0xc0u));
    f->dx = (unsigned short)((max_head << 8) | 0x01u);
    rm_clear_cf(f);
}

static void hdd_chs_rw(struct rm_int13_frame* f, unsigned char ah) {
    struct legacy_hdd_geometry geometry;
    unsigned int count = (unsigned char)f->ax;
    unsigned int cylinder =
        ((unsigned int)(f->cx >> 8) | (((unsigned int)f->cx & 0x00c0u) << 2));
    unsigned int sector = (unsigned int)(f->cx & 0x003fu);
    unsigned int head = (unsigned int)(f->dx >> 8);
    unsigned int lba;
    unsigned int dest;

    legacy_hdd_get_geometry(&geometry);
    if (ah != 0x02u || sector == 0u || count == 0u || head >= geometry.heads ||
        cylinder >= geometry.cylinders) {
        f->ax = 0x0100u;
        rm_set_cf(f);
        return;
    }

    lba = ((cylinder * geometry.heads) + head) * geometry.sectors_per_track +
          sector - 1u;
    dest = rm_seg_off_to_linear(f->es, f->bx);
    if (legacy_hdd_read_sectors(lba, count, dest) != 0) {
        f->ax = 0x0100u;
        rm_set_cf(f);
        return;
    }
    f->ax = (unsigned short)count;
    rm_clear_cf(f);
}

static void hdd_ext_read(struct rm_int13_frame* f) {
    struct rm_dap* dap = (struct rm_dap*)rm_seg_off_to_linear(f->ds, f->si);
    unsigned int dest;

    if (dap->size < 0x10u || dap->lba_high != 0u || dap->count == 0u) {
        legacy_serial_write_string("13h:42 bad dap size=");
        legacy_serial_write_hex8(dap->size);
        legacy_serial_write_string(" cnt=");
        legacy_serial_write_hex16(dap->count);
        legacy_serial_write_string(" high=");
        legacy_serial_write_hex32(dap->lba_high);
        legacy_serial_write_string("\r\n");
        f->ax = 0x0100u;
        rm_set_cf(f);
        return;
    }
    dest = rm_seg_off_to_linear(dap->seg, dap->off);
    if (legacy_hdd_read_sectors(dap->lba_low, dap->count, dest) != 0) {
        f->ax = 0x0100u;
        rm_set_cf(f);
        return;
    }
    f->ax = 0u;
    rm_clear_cf(f);
}

static void hdd_ext_params(struct rm_int13_frame* f) {
    struct rm_edd_params* p =
        (struct rm_edd_params*)rm_seg_off_to_linear(f->ds, f->si);
    struct legacy_hdd_geometry geometry;
    unsigned int fill_size;

    legacy_hdd_get_geometry(&geometry);
    if (p->size < 0x1au) {
        f->ax = 0x0100u;
        rm_set_cf(f);
        return;
    }

    fill_size = p->size;
    if (fill_size > sizeof(*p)) {
        fill_size = sizeof(*p);
    }
    legacy_memset(p, 0u, fill_size);
    p->size = (unsigned short)fill_size;
    p->information = 0x0001u;
    p->cylinders = geometry.cylinders;
    p->heads = geometry.heads;
    p->sectors = geometry.sectors_per_track;
    p->total_sectors_low = geometry.total_sectors;
    p->total_sectors_high = 0u;
    p->bytes_per_sector = 512u;
    if (fill_size >= sizeof(*p)) {
        p->edd_config_params = 0u;
    }
    f->ax = 0u;
    rm_clear_cf(f);
}

static void hdd_service(struct rm_int13_frame* f) {
    unsigned char ah = (unsigned char)(f->ax >> 8);
    unsigned char dl = (unsigned char)f->dx;
    struct legacy_hdd_geometry geometry;

    if (dl != 0x80u || !legacy_hdd_is_present()) {
        f->ax = 0x0100u;
        rm_set_cf(f);
        return;
    }
    legacy_hdd_get_geometry(&geometry);

    switch (ah) {
        case 0x00u:
            f->ax &= 0x00ffu;
            rm_clear_cf(f);
            return;
        case 0x08u:
            hdd_params(f);
            return;
        case 0x15u:
            f->ax = (unsigned short)((0x03u << 8) | (f->ax & 0x00ffu));
            f->cx = (unsigned short)(geometry.total_sectors >> 16);
            f->dx = (unsigned short)geometry.total_sectors;
            rm_clear_cf(f);
            return;
        case 0x02u:
        case 0x03u:
            hdd_chs_rw(f, ah);
            return;
        case 0x41u:
            if (f->bx != 0x55aau) {
                f->ax = 0x0100u;
                rm_set_cf(f);
                return;
            }
            f->bx = 0xaa55u;
            f->cx = 0x0001u;
            f->ax = 0x3000u;
            rm_clear_cf(f);
            return;
        case 0x42u:
            hdd_ext_read(f);
            return;
        case 0x48u:
            hdd_ext_params(f);
            return;
        default:
            f->ax = 0x0100u;
            rm_set_cf(f);
            return;
    }
}

void legacy_int13_service(struct rm_int13_frame* f,
                          unsigned int floppy_dpt_linear) {
    unsigned char dl = (unsigned char)(f->dx & 0xffu);

    if (dl >= 0x80u) {
        hdd_service(f);
        return;
    }

    floppy_service(f, floppy_dpt_linear);
}
