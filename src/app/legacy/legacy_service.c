#include "app/legacy/legacy_service.h"

#include "app/legacy/legacy_boot.h"
#include "app/legacy/legacy_debug.h"
#include "app/legacy/legacy_int13.h"
#include "app/legacy/legacy_int15.h"
#include "app/legacy/legacy_keyboard.h"
#include "app/legacy/legacy_misc.h"
#include "app/legacy/legacy_platform.h"
#include "app/legacy/legacy_rm.h"
#include "app/legacy/legacy_thunk.h"
#include "app/legacy/legacy_timer.h"
#include "app/legacy/legacy_time.h"
#include "app/legacy/legacy_video.h"

static struct legacy_service_context legacy_context;

static void rm_set_cf(struct rm_int13_frame* f) { f->flags |= 0x0001u; }

void legacy_service_init(const struct legacy_service_context* context) {
    legacy_context = *context;
}

void bios_rm_service(unsigned int vector, struct rm_int13_frame* f) {
    legacy_timer_update();
    if (0 && vector != 0x16 && vector != 0x10) {
        legacy_serial_write_string("bios_rm_service=");
        legacy_serial_write_hex8(vector & 0xffu);
        legacy_serial_write_string(", ah=");
        legacy_serial_write_hex8(f->ax >> 8);
        legacy_serial_write_string("\r\n");
    }
    switch (vector & 0xffu) {
        case 0x10:
            legacy_int10_service(f);
            return;
        case 0x11:
            legacy_int11_service(f);
            return;
        case 0x12:
            legacy_int12_service(f, legacy_context.base_mem_kb);
            return;
        case 0x13:
        case 0x40:
            legacy_int13_service(f, legacy_context.floppy_dpt_linear);
            return;
        case 0x15:
            legacy_int15_service(f, legacy_context.total_bytes);
            return;
        case 0x16:
            legacy_int16_service(f);
            return;
        case 0x17:
            legacy_int17_service(f);
            return;
        case 0x19: {
            unsigned char boot_drive = legacy_prepare_boot_sector(
                legacy_context.boot_priority, legacy_context.record_boot_success);
            legacy_install_boot_drive(boot_drive);
            if (legacy_context.boot_pm32 != 0) {
                legacy_context.boot_pm32();
            }
            return;
        }
        case 0x1a:
            legacy_int1a_service(f, legacy_timer_tick_counter());
            return;
        case 0x60:
            legacy_int60_service(f);
            return;
        default:
            f->ax = (unsigned short)((0x86u << 8) | (f->ax & 0x00ffu));
            rm_set_cf(f);
            return;
    }
}
