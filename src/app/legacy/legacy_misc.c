#include "app/legacy/legacy_misc.h"

static void rm_set_cf(struct rm_int13_frame* f) { f->flags |= 0x0001u; }

void legacy_int11_service(struct rm_int13_frame* f) {
    f->ax = *(volatile unsigned short*)0x0410u;
}

void legacy_int12_service(struct rm_int13_frame* f,
                          unsigned short base_mem_kb) {
    f->ax = base_mem_kb;
}

void legacy_int17_service(struct rm_int13_frame* f) { rm_set_cf(f); }
