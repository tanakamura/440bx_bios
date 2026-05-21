#include "legacy_rm.h"

void bios_rm_service(unsigned int vector, struct rm_int13_frame* f) {
    (void)vector;
    if (f != 0) {
        f->ax = (unsigned short)((0x86u << 8) | (f->ax & 0x00ffu));
        f->flags |= 0x0001u;
    }
}
