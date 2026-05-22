#include "app/legacy/legacy_vgabios_vm86.h"

#include "bios_io.h"
#include "bios_serial.h"
#include "vm86.h"

#ifndef LEGACY_VGABIOS_USE_VM86
#define LEGACY_VGABIOS_USE_VM86 1
#endif

#define VM86_SENTINEL_SEG 0x0700u
#define VM86_SENTINEL_OFF 0x0000u
#define VBIOS_VM86_SS 0x0000u
#define VBIOS_VM86_SP 0x8ff0u

extern void bios_rm_service(unsigned int vector, struct rm_int13_frame* f);
extern void bios_call_vgabios_init_pm32(unsigned int bdf);

static void legacy_vgabios_bios_int(unsigned char vector,
                                    struct rm_int13_frame* frame,
                                    void* opaque) {
    (void)opaque;
    bios_rm_service(vector, frame);
}

static unsigned int legacy_vgabios_io_in(unsigned short port, unsigned int size,
                                         void* opaque) {
    (void)opaque;
    if (size == 1u) {
        return inb(port);
    }
    if (size == 2u) {
        return inw(port);
    }
    return inl(port);
}

static void legacy_vgabios_io_out(unsigned short port, unsigned int size,
                                  unsigned int value, void* opaque) {
    (void)opaque;
    if (size == 1u) {
        outb(port, (unsigned char)value);
    } else if (size == 2u) {
        outw(port, (unsigned short)value);
    } else {
        outl(port, value);
    }
}

void legacy_vgabios_init_vm86(unsigned int bdf) {
    volatile unsigned short* vm_stack =
        (volatile unsigned short*)((unsigned int)VBIOS_VM86_SS * 16u +
                                   (unsigned int)VBIOS_VM86_SP);
    struct vm86_run_spec spec = {
        .regs =
            {
                .eax = bdf & 0xffffu,
                .ebx = 0xffffu,
                .ecx = 0u,
                .edx = 0xffffu,
                .esi = 0u,
                .edi = 0u,
                .ebp = 0u,
                .eflags16 = 0x0202u,
                .es = 0u,
                .ds = 0u,
                .fs = 0u,
                .gs = 0u,
                .ss = VBIOS_VM86_SS,
                .sp = VBIOS_VM86_SP,
                .cs = 0xc000u,
                .ip = 0x0003u,
            },
        .io =
            {
                .in = legacy_vgabios_io_in,
                .out = legacy_vgabios_io_out,
            },
        .ints =
            {
                .bios_int = legacy_vgabios_bios_int,
            },
        .opaque = 0,
        .thunk_linear_base = 0x000fe000u,
    };
    vm_stack[0] = VM86_SENTINEL_OFF;
    vm_stack[1] = VM86_SENTINEL_SEG;
    int rc = vm86_run(&spec);
    if (rc != 0) {
        serial_write_string("vm86 optionrom rc=");
        serial_write_hex8((unsigned char)rc);
        serial_write_string("\r\n");
    }
}

void legacy_vgabios_init(unsigned int bdf) {
#if LEGACY_VGABIOS_USE_VM86
    legacy_vgabios_init_vm86(bdf);
#else
    bios_call_vgabios_init_pm32(bdf);
#endif
}
