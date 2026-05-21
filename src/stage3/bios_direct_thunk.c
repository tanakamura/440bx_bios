#include "bios_direct_thunk.h"

extern unsigned char bios16_thunk_start[];
extern unsigned char bios16_default[];
extern unsigned char bios16_iret[];
extern unsigned char bios16_thunk_end[];
extern unsigned int bios16_pm_stack_top;
extern unsigned char bios16_boot_drive[];
extern unsigned char bios16_vbe_mode_info[];

static void serialize_instruction_stream(void) {
    __asm__ volatile("jmp 1f\n1:" : : : "memory");
}

static void write_linear16(unsigned int linear, unsigned short value) {
    __asm__ volatile("movw %w0, (%1)"
                     :
                     : "r"(value), "r"(linear)
                     : "memory");
}

static void install_ivt_vector(unsigned char vector, unsigned int linear) {
    unsigned short offset;
    unsigned short segment;
    unsigned int ivt_linear = (unsigned int)vector * 4u;

    if (linear >= 0x000ffff0u && linear <= 0x0010ffefu) {
        segment = 0xffffu;
        offset = (unsigned short)(linear - 0x000ffff0u);
    } else {
        segment = (unsigned short)(linear >> 4);
        offset = (unsigned short)(linear & 0x000fu);
    }

    write_linear16(ivt_linear, offset);
    write_linear16(ivt_linear + 2u, segment);
}

static void install_thunk_vector(unsigned char vector, unsigned int linear) {
    unsigned short segment =
        (unsigned short)(BIOS_DIRECT_THUNK_RUNTIME_BASE >> 4);
    unsigned short offset =
        (unsigned short)(linear - BIOS_DIRECT_THUNK_RUNTIME_BASE);
    unsigned int ivt_linear = (unsigned int)vector * 4u;

    write_linear16(ivt_linear, offset);
    write_linear16(ivt_linear + 2u, segment);
}

static void copy_thunk_code(bios_direct_thunk_void_fn install_shadow) {
    volatile unsigned char* thunk =
        (volatile unsigned char*)BIOS_DIRECT_THUNK_RUNTIME_BASE;
    unsigned int thunk_size =
        (unsigned int)(bios16_thunk_end - bios16_thunk_start);
    unsigned int thunk_off;

    if (install_shadow != 0) {
        install_shadow();
    }

    for (thunk_off = 0; thunk_off < thunk_size; ++thunk_off) {
        thunk[thunk_off] = bios16_thunk_start[thunk_off];
    }
}

void bios_direct_thunk_install(bios_direct_thunk_void_fn install_shadow) {
    unsigned int i;
    unsigned int default_linear;

    copy_thunk_code(install_shadow);
    default_linear =
        BIOS_DIRECT_THUNK_RUNTIME_BASE +
        (unsigned int)(bios16_default - bios16_thunk_start);

    for (i = 0; i < 256u; ++i) {
        install_ivt_vector((unsigned char)i, default_linear);
    }
    install_thunk_vector(
        0x1c, BIOS_DIRECT_THUNK_RUNTIME_BASE +
                  (unsigned int)(bios16_iret - bios16_thunk_start));
    serialize_instruction_stream();
}

void bios_direct_thunk_install_boot_drive(unsigned char boot_drive) {
    *(volatile unsigned char*)(BIOS_DIRECT_THUNK_RUNTIME_BASE +
                               (unsigned int)(bios16_boot_drive -
                                              bios16_thunk_start)) =
        boot_drive;
}

void bios_direct_thunk_install_pm_stack_top(unsigned int stack_top) {
    *(volatile unsigned int*)(BIOS_DIRECT_THUNK_RUNTIME_BASE +
                              (unsigned int)((unsigned char*)
                                                 &bios16_pm_stack_top -
                                             bios16_thunk_start)) = stack_top;
}

unsigned char* bios_direct_thunk_vbe_mode_info_buffer(void) {
    return (unsigned char*)(BIOS_DIRECT_THUNK_RUNTIME_BASE +
                            (unsigned int)(bios16_vbe_mode_info -
                                           bios16_thunk_start));
}
