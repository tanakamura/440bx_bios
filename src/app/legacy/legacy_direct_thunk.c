#include "app/legacy/legacy_direct_thunk.h"

extern unsigned char bios16_thunk_start[];
extern unsigned char bios16_int08[];
extern unsigned char bios16_int10[];
extern unsigned char bios16_int11[];
extern unsigned char bios16_int12[];
extern unsigned char bios16_int13[];
extern unsigned char bios16_int15[];
extern unsigned char bios16_int16[];
extern unsigned char bios16_int17[];
extern unsigned char bios16_int19[];
extern unsigned char bios16_int1a[];
extern unsigned char bios16_int42[];
extern unsigned char bios16_int60[];
extern unsigned char bios16_int6d[];
extern unsigned char bios16_unknown_stubs_start[];
extern unsigned char bios16_unknown_stubs_end[];
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
        (unsigned short)(LEGACY_DIRECT_THUNK_RUNTIME_BASE >> 4);
    unsigned short offset =
        (unsigned short)(linear - LEGACY_DIRECT_THUNK_RUNTIME_BASE);
    unsigned int ivt_linear = (unsigned int)vector * 4u;

    write_linear16(ivt_linear, offset);
    write_linear16(ivt_linear + 2u, segment);
}

static void copy_thunk_code(legacy_direct_thunk_void_fn install_shadow) {
    volatile unsigned char* thunk =
        (volatile unsigned char*)LEGACY_DIRECT_THUNK_RUNTIME_BASE;
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

void legacy_direct_thunk_install(legacy_direct_thunk_void_fn install_shadow) {
    unsigned int i;
    unsigned int unknown_stubs_linear;
    unsigned int unknown_stub_size;

    copy_thunk_code(install_shadow);
    unknown_stubs_linear =
        LEGACY_DIRECT_THUNK_RUNTIME_BASE +
        (unsigned int)(bios16_unknown_stubs_start - bios16_thunk_start);
    unknown_stub_size =
        (unsigned int)(bios16_unknown_stubs_end - bios16_unknown_stubs_start) /
        256u;

    for (i = 0; i < 256u; ++i) {
        install_ivt_vector((unsigned char)i,
                           unknown_stubs_linear + i * unknown_stub_size);
    }
    install_thunk_vector(
        0x1c, LEGACY_DIRECT_THUNK_RUNTIME_BASE +
                  (unsigned int)(bios16_iret - bios16_thunk_start));
    install_thunk_vector(
        0x08, LEGACY_DIRECT_THUNK_RUNTIME_BASE +
                  (unsigned int)(bios16_int08 - bios16_thunk_start));
    install_thunk_vector(
        0x10, LEGACY_DIRECT_THUNK_RUNTIME_BASE +
                  (unsigned int)(bios16_int10 - bios16_thunk_start));
    install_thunk_vector(
        0x11, LEGACY_DIRECT_THUNK_RUNTIME_BASE +
                  (unsigned int)(bios16_int11 - bios16_thunk_start));
    install_thunk_vector(
        0x12, LEGACY_DIRECT_THUNK_RUNTIME_BASE +
                  (unsigned int)(bios16_int12 - bios16_thunk_start));
    install_thunk_vector(
        0x13, LEGACY_DIRECT_THUNK_RUNTIME_BASE +
                  (unsigned int)(bios16_int13 - bios16_thunk_start));
    install_thunk_vector(
        0x15, LEGACY_DIRECT_THUNK_RUNTIME_BASE +
                  (unsigned int)(bios16_int15 - bios16_thunk_start));
    install_thunk_vector(
        0x16, LEGACY_DIRECT_THUNK_RUNTIME_BASE +
                  (unsigned int)(bios16_int16 - bios16_thunk_start));
    install_thunk_vector(
        0x17, LEGACY_DIRECT_THUNK_RUNTIME_BASE +
                  (unsigned int)(bios16_int17 - bios16_thunk_start));
    install_thunk_vector(
        0x19, LEGACY_DIRECT_THUNK_RUNTIME_BASE +
                  (unsigned int)(bios16_int19 - bios16_thunk_start));
    install_thunk_vector(
        0x1a, LEGACY_DIRECT_THUNK_RUNTIME_BASE +
                  (unsigned int)(bios16_int1a - bios16_thunk_start));
    install_thunk_vector(
        0x42, LEGACY_DIRECT_THUNK_RUNTIME_BASE +
                  (unsigned int)(bios16_int42 - bios16_thunk_start));
    install_thunk_vector(
        0x60, LEGACY_DIRECT_THUNK_RUNTIME_BASE +
                  (unsigned int)(bios16_int60 - bios16_thunk_start));
    install_thunk_vector(
        0x6d, LEGACY_DIRECT_THUNK_RUNTIME_BASE +
                  (unsigned int)(bios16_int6d - bios16_thunk_start));
    serialize_instruction_stream();
}

void legacy_direct_thunk_install_boot_drive(unsigned char boot_drive) {
    *(volatile unsigned char*)(LEGACY_DIRECT_THUNK_RUNTIME_BASE +
                               (unsigned int)(bios16_boot_drive -
                                              bios16_thunk_start)) =
        boot_drive;
}

void legacy_direct_thunk_install_pm_stack_top(unsigned int stack_top) {
    *(volatile unsigned int*)(LEGACY_DIRECT_THUNK_RUNTIME_BASE +
                              (unsigned int)((unsigned char*)
                                                 &bios16_pm_stack_top -
                                             bios16_thunk_start)) = stack_top;
}

unsigned char* legacy_direct_thunk_vbe_mode_info_buffer(void) {
    return (unsigned char*)(LEGACY_DIRECT_THUNK_RUNTIME_BASE +
                            (unsigned int)(bios16_vbe_mode_info -
                                           bios16_thunk_start));
}
