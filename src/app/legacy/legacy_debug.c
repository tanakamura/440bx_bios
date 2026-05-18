#include "app/legacy/legacy_debug.h"

static void rm_set_cf(struct rm_int13_frame* f) { f->flags |= 0x0001u; }

static void rm_clear_cf(struct rm_int13_frame* f) { f->flags &= 0xfffeu; }

void legacy_int60_service(struct rm_int13_frame* f) {
    unsigned int linear = ((unsigned int)f->cx << 16) | (unsigned int)f->dx;

    switch ((unsigned char)(f->ax >> 8)) {
        case 0x00u:
            f->ax = (unsigned short)((f->ax & 0xff00u) |
                                     (*(volatile unsigned char*)linear));
            rm_clear_cf(f);
            return;
        case 0x01u:
            *(volatile unsigned char*)linear = (unsigned char)(f->ax & 0x00ffu);
            rm_clear_cf(f);
            return;
        default:
            f->ax = (unsigned short)((0x86u << 8) | (f->ax & 0x00ffu));
            rm_set_cf(f);
            return;
    }
}
