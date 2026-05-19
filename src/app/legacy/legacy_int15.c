#include "legacy_int15.h"
#include "legacy_platform.h"

#define E820_SMAP 0x534d4150u

static unsigned int rm_seg_off_to_linear(unsigned short seg,
                                         unsigned short off) {
    return ((unsigned int)seg << 4) + off;
}

static void rm_set_cf(struct rm_int13_frame* f) { f->flags |= 0x0001u; }

static void rm_clear_cf(struct rm_int13_frame* f) { f->flags &= 0xfffeu; }

static void rm_return_eax32(struct rm_int13_frame* f, unsigned int value) {
    f->eax32 = value;
    f->ax = (unsigned short)value;
}

static void rm_return_ebx32(struct rm_int13_frame* f, unsigned int value) {
    f->ebx32 = value;
    f->bx = (unsigned short)value;
}

static void rm_return_ecx32(struct rm_int13_frame* f, unsigned int value) {
    f->ecx32 = value;
    f->cx = (unsigned short)value;
}

static void rm_return_edx32(struct rm_int13_frame* f, unsigned int value) {
    f->edx32 = value;
    f->dx = (unsigned short)value;
}

static void int15_e820(struct rm_int13_frame* f, unsigned int total_bytes) {
    unsigned int index = f->ebx32;
    unsigned int count = legacy_memory_e820_entry_count(total_bytes);
    struct legacy_e820_entry* entry;

    if (f->edx32 != E820_SMAP || f->ecx32 < sizeof(*entry) || index >= count) {
        rm_return_eax32(f, 0x00008600u);
        rm_set_cf(f);
        return;
    }

    entry = (struct legacy_e820_entry*)rm_seg_off_to_linear(f->es, f->di);
    if (legacy_memory_e820_get_entry(total_bytes, index, entry) != 0) {
        rm_return_eax32(f, 0x00008600u);
        rm_set_cf(f);
        return;
    }

    rm_return_eax32(f, E820_SMAP);
    rm_return_edx32(f, E820_SMAP);
    rm_return_ecx32(f, sizeof(*entry));
    rm_return_ebx32(f, (index + 1u < count) ? index + 1u : 0u);
    rm_clear_cf(f);
}

void legacy_int15_service(struct rm_int13_frame* f, unsigned int total_bytes) {
    if (f->ax == 0xe820u) {
        int15_e820(f, total_bytes);
        return;
    }
    if ((unsigned char)(f->ax >> 8) == 0x88u) {
        unsigned int kb = 0u;
        unsigned int usable_end = legacy_memory_extended_usable_end(total_bytes);
        if (usable_end > 0x00100000u) {
            kb = (usable_end - 0x00100000u) >> 10;
            if (kb > 0xffffu) {
                kb = 0xffffu;
            }
        }
        f->ax = (unsigned short)kb;
        rm_clear_cf(f);
        return;
    }
    if (f->ax == 0xe801u) {
        unsigned int usable_end = legacy_memory_extended_usable_end(total_bytes);
        unsigned int below16m_kb = 0u;
        unsigned int above16m_64k = 0u;
        if (usable_end > 0x00100000u) {
            unsigned int below_end = usable_end;
            if (below_end > 0x01000000u) {
                below_end = 0x01000000u;
            }
            below16m_kb = (below_end - 0x00100000u) >> 10;
        }
        if (usable_end > 0x01000000u) {
            above16m_64k = (usable_end - 0x01000000u) >> 16;
        }
        f->ax = (unsigned short)below16m_kb;
        f->bx = (unsigned short)above16m_64k;
        f->cx = (unsigned short)below16m_kb;
        f->dx = (unsigned short)above16m_64k;
        rm_clear_cf(f);
        return;
    }
    if ((unsigned char)(f->ax >> 8) == 0x86u) {
        rm_clear_cf(f);
        return;
    }
    f->ax = (unsigned short)((0x86u << 8) | (f->ax & 0x00ffu));
    rm_set_cf(f);
}
