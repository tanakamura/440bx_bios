#include "bios_legacy.h"

#include "bios_app_loader.h"
#include "bios_direct_thunk.h"
#include "bios_pci.h"
#include "bios_serial.h"
#include "legacy_app_abi.h"
#include "legacy_rm.h"

static const unsigned short bios_ebda_segment = 0x0000u;
static const unsigned short bios_dos_base_mem_kb = 640u;
static unsigned char rm_unsupported_log_count;

typedef void (*legacy_app_entry_fn)(const struct legacy_runtime_config* config);

static struct legacy_app_exports bios_legacy_exports;

static unsigned int lowmem_addr(unsigned int addr) {
    __asm__ volatile("" : "+r"(addr));
    return addr;
}

static volatile unsigned char* lowmem_u8(unsigned int linear) {
    return (volatile unsigned char*)lowmem_addr(linear);
}

static volatile unsigned short* lowmem_u16(unsigned int linear) {
    return (volatile unsigned short*)lowmem_addr(linear);
}

static void direct_bda_video_init(void) {
    unsigned int i;

    *lowmem_u16(0x0413u) = bios_dos_base_mem_kb;
    *lowmem_u16(0x040eu) = bios_ebda_segment;
    *lowmem_u8(0x0449u) = 0x03u;
    *lowmem_u16(0x044au) = 80u;
    *lowmem_u16(0x044cu) = 0x1000u;
    *lowmem_u16(0x044eu) = 0x0000u;
    *lowmem_u16(0x0460u) = 0x0607u;
    *lowmem_u8(0x0462u) = 0x00u;
    *lowmem_u16(0x0463u) = 0x03d4u;
    *lowmem_u8(0x0465u) = 0x09u;
    *lowmem_u8(0x0466u) = 0x00u;
    *lowmem_u8(0x0484u) = 24u;
    *lowmem_u8(0x0485u) = 16u;
    *lowmem_u8(0x0487u) = 0x00u;
    *lowmem_u8(0x0488u) = 0x00u;
    *lowmem_u8(0x048au) = 0x08u;
    for (i = 0u; i < 8u; ++i) {
        *lowmem_u16(0x0450u + i * 2u) = 0x0000u;
    }
}

void bios_legacy_install_runtime(
    const struct bios_stage_context* stage, unsigned char boot_priority,
    bios_legacy_record_boot_success_fn record_boot_success,
    bios_legacy_void_fn boot_pm32, bios_legacy_void_fn install_shadow) {
    struct legacy_runtime_config config;

    config.total_bytes = stage->total_bytes;
    config.base_mem_kb = bios_dos_base_mem_kb;
    config.ebda_segment = bios_ebda_segment;
    config.boot_priority = boot_priority;
    config.record_boot_success = record_boot_success;
    config.boot_pm32 = boot_pm32;
    config.install_shadow = install_shadow;
    config.exports = &bios_legacy_exports;
    bios_legacy_exports = (struct legacy_app_exports){0};

    if (stage->legacy_app_blob_linear != 0u) {
        unsigned int load_addr = bios_app_load(
            stage, SHARED_PAYLOAD_ID_LEGACY_APP, "Legacy");
        if (load_addr != 0u) {
            ((legacy_app_entry_fn)load_addr)(&config);
            return;
        }
    }

    serial_write_string("Legacy app missing; direct thunk only\r\n");
    direct_bda_video_init();
    bios_direct_thunk_install(install_shadow);
}

static int legacy_exports_ready(void) {
    return bios_legacy_exports.magic == LEGACY_APP_EXPORTS_MAGIC &&
           bios_legacy_exports.version == LEGACY_APP_EXPORTS_VERSION &&
           bios_legacy_exports.size >= sizeof(bios_legacy_exports);
}

unsigned char bios_legacy_prepare_boot_sector(
    unsigned char boot_priority,
    bios_legacy_record_boot_success_fn record_boot_success) {
    if (legacy_exports_ready() &&
        bios_legacy_exports.prepare_boot_sector != 0) {
        return bios_legacy_exports.prepare_boot_sector(boot_priority,
                                                       record_boot_success);
    }
    serial_write_string("Legacy app missing for boot\r\n");
    for (;;) {
        __asm__ volatile("hlt");
    }
}

void bios_legacy_install_boot_drive(unsigned char boot_drive) {
    if (legacy_exports_ready() && bios_legacy_exports.install_boot_drive != 0) {
        bios_legacy_exports.install_boot_drive(boot_drive);
        return;
    }
    bios_direct_thunk_install_boot_drive(boot_drive);
}

void bios_legacy_install_pm_stack_top(unsigned int pm_stack_top) {
    if (legacy_exports_ready() &&
        bios_legacy_exports.install_pm_stack_top != 0) {
        bios_legacy_exports.install_pm_stack_top(pm_stack_top);
        return;
    }
    bios_direct_thunk_install_pm_stack_top(pm_stack_top);
}

void bios_rm_service(unsigned int vector, struct rm_int13_frame* f) {
    unsigned int vec = vector & 0xffu;

    if (f == 0) {
        return;
    }

    switch (vec) {
        case 0x10:
            if ((unsigned char)(f->ax >> 8) == 0x0fu) {
                f->ax = 0x5003u;
                f->bx &= 0x00ffu;
                f->flags &= 0xfffeu;
                return;
            }
            if ((unsigned char)(f->ax >> 8) == 0x03u) {
                f->cx = *lowmem_u16(0x0460u);
                f->dx = *lowmem_u16(0x0450u + ((f->bx >> 8) & 7u) * 2u);
                f->flags &= 0xfffeu;
                return;
            }
            if ((unsigned char)(f->ax >> 8) == 0x02u) {
                *lowmem_u16(0x0450u + ((f->bx >> 8) & 7u) * 2u) = f->dx;
                f->flags &= 0xfffeu;
                return;
            }
            if ((unsigned char)(f->ax >> 8) == 0x0eu) {
                serial_write_char((char)(f->ax & 0x00ffu));
                f->flags &= 0xfffeu;
                return;
            }
            break;
        case 0x11:
            f->ax = *lowmem_u16(0x0410u);
            f->flags &= 0xfffeu;
            return;
        case 0x12:
            f->ax = bios_dos_base_mem_kb;
            f->flags &= 0xfffeu;
            return;
        case 0x1a:
            /* PCI BIOS services used by some option ROM initializers. */
            if ((unsigned char)(f->ax >> 8) == 0xb1u) {
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
                    f->flags &= 0xfffeu;
                    return;
                }

                if (sub == 0x02u || sub == 0x03u) {
                    unsigned int wanted =
                        sub == 0x02u
                            ? (((unsigned int)f->cx << 16) | f->dx)
                            : (f->ecx32 & 0x00ffffffu);
                    unsigned int seen = 0u;
                    unsigned char b;
                    unsigned char d;
                    unsigned char n;

                    if (sub == 0x02u && f->dx == 0xffffu) {
                        f->ax = (unsigned short)(0x8300u | sub);
                        f->flags |= 0x0001u;
                        return;
                    }

                    index = f->si;
                    for (b = 0u; b <= last_bus; ++b) {
                        for (d = 0u; d < 32u; ++d) {
                            for (n = 0u; n < 8u; ++n) {
                                unsigned int id = pci_read32(b, d, n, 0x00u);
                                if ((id & 0xffffu) == 0xffffu) {
                                    continue;
                                }
                                value = sub == 0x02u
                                            ? id
                                            : (pci_read32(b, d, n, 0x08u) >> 8);
                                if (value != wanted) {
                                    continue;
                                }
                                if (seen++ != index) {
                                    continue;
                                }
                                f->ax = (unsigned short)(f->ax & 0x00ffu);
                                f->bx =
                                    (unsigned short)(((unsigned short)b << 8) |
                                                     (unsigned short)((d << 3) |
                                                                      n));
                                f->flags &= 0xfffeu;
                                return;
                            }
                        }
                    }
                    f->ax = (unsigned short)(0x8600u | sub);
                    f->flags |= 0x0001u;
                    return;
                }

                if (sub >= 0x08u && sub <= 0x0du) {
                    if ((reg & ((sub == 0x09u || sub == 0x0cu) ? 1u :
                                (sub == 0x0au || sub == 0x0du) ? 3u : 0u)) !=
                        0u) {
                        f->ax = (unsigned short)(0x8700u | sub);
                        f->flags |= 0x0001u;
                        return;
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
                    f->flags &= 0xfffeu;
                    return;
                }

                f->ax = (unsigned short)(0x8100u | sub);
                f->flags |= 0x0001u;
                return;
            }
            break;
        default:
            break;
    }

    if (rm_unsupported_log_count < 16u) {
        ++rm_unsupported_log_count;
        serial_write_string("RM unsupported int=");
        serial_write_hex8((unsigned char)vec);
        serial_write_string(" ax=");
        serial_write_hex16(f->ax);
        serial_write_string("\r\n");
    }
    f->ax = (unsigned short)((0x86u << 8) | (f->ax & 0x00ffu));
    f->flags |= 0x0001u;
}
