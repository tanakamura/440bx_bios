#include "post_code.h"
#include "bios_io.h"
#include "bios_pci.h"
#include "bios_serial.h"
#include "bios_storage.h"

#define STORAGE_SECTOR_OFF 0x00000u
#define STORAGE_ID_OFF 0x01000u
#define USB_FRAME_LIST_OFF 0x02000u
#define USB_QH_OFF 0x03000u
#define USB_TD_OFF 0x04000u
#define USB_BUF_OFF 0x08000u
#define USB_CFG_BUF_OFF 0x08400u
#define USB_CBW_BUF_OFF 0x08800u
#define USB_CSW_BUF_OFF 0x08900u
#define USB_SECTOR_OFF 0x09000u
#define IDE_PRD_OFF 0x0a000u

static unsigned int storage_scratch_base = 0x00500000u;

static unsigned int storage_addr(unsigned int off) {
    return storage_scratch_base + off;
}

#define STORAGE_SECTOR_LINEAR storage_addr(STORAGE_SECTOR_OFF)
#define STORAGE_ID_LINEAR storage_addr(STORAGE_ID_OFF)
#define USB_FRAME_LIST_LINEAR storage_addr(USB_FRAME_LIST_OFF)
#define USB_QH_LINEAR storage_addr(USB_QH_OFF)
#define USB_TD_LINEAR storage_addr(USB_TD_OFF)
#define USB_BUF_LINEAR storage_addr(USB_BUF_OFF)
#define USB_CFG_BUF_LINEAR storage_addr(USB_CFG_BUF_OFF)
#define USB_CBW_BUF_LINEAR storage_addr(USB_CBW_BUF_OFF)
#define USB_CSW_BUF_LINEAR storage_addr(USB_CSW_BUF_OFF)
#define USB_SECTOR_LINEAR storage_addr(USB_SECTOR_OFF)
#define IDE_PRD_LINEAR storage_addr(IDE_PRD_OFF)
#define IDE_MAX_PRD 32u
#define USB_MAX_TD 256u

struct storage_pci_bdf {
    unsigned char bus;
    unsigned char dev;
    unsigned char fn;
};

struct usb_dev {
    unsigned short io;
    unsigned char addr;
    unsigned char low_speed;
    unsigned char ep0_mps;
    unsigned char bulk_in;
    unsigned char bulk_out;
    unsigned short bulk_in_mps;
    unsigned short bulk_out_mps;
    unsigned char bulk_in_toggle;
    unsigned char bulk_out_toggle;
    unsigned char interface_number;
};

struct bios_hdd_candidate {
    unsigned char present;
    unsigned char kind;
    unsigned char has_mbr;
    unsigned short io;
    unsigned short ctrl;
    unsigned short bmio;
    unsigned char drive;
    unsigned char dma_enabled;
    unsigned char lba48_dma_enabled;
    struct usb_dev usb_dev;
    unsigned int total_sectors;
    unsigned short heads;
    unsigned short spt;
    unsigned short cylinders;
};

static struct storage_pci_bdf storage_ide_bdf;
static struct storage_pci_bdf storage_uhci_bdf;
static unsigned char storage_ide_found = 0;
static unsigned char storage_uhci_found = 0;
static unsigned int pci_next_io = 0xd000u;
static unsigned int pci_next_mem = 0xf0000000u;
static unsigned char usb_msd_quiet_status = 0;
static unsigned char bios_hdd_present = 0;
static unsigned char bios_hdd_kind = 0;
static unsigned char bios_hdd_has_mbr = 0;
static unsigned short bios_hdd_io = 0;
static unsigned short bios_hdd_ctrl = 0;
static unsigned short bios_hdd_bmio = 0;
static unsigned char bios_hdd_drive = 0;
static unsigned char bios_hdd_dma_enabled = 0;
static unsigned char bios_hdd_lba48_dma_enabled = 0;
static unsigned char bios_hdd_dma_fallback_logged = 0;
static unsigned char bios_hdd_dma_ext_fallback_logged = 0;
static struct usb_dev bios_hdd_usb_dev;
static unsigned int bios_hdd_total_sectors = 0;
static unsigned short bios_hdd_heads = 16;
static unsigned short bios_hdd_spt = 63;
static unsigned short bios_hdd_cylinders = 1;
static unsigned short storage_ide_bmiba = 0;
static unsigned char storage_ide_udmactl = 0;
static unsigned short storage_ide_udmatim = 0;
static struct bios_hdd_candidate bios_hdd_ide_candidate;
static struct bios_hdd_candidate bios_hdd_usb_candidate;

void storage_set_scratch_base(unsigned int base) {
    storage_scratch_base = base & ~0xfffu;
}

static void storage_memset(void* dst, unsigned char value, unsigned int len) {
    unsigned char* p = (unsigned char*)dst;
    while (len-- != 0u) {
        *p++ = value;
    }
}

static void storage_memcpy(void* dst, const void* src, unsigned int len) {
    unsigned char* d = (unsigned char*)dst;
    const unsigned char* s = (const unsigned char*)src;
    while (len-- != 0u) {
        *d++ = *s++;
    }
}

static unsigned short le16(const unsigned char* p) {
    return (unsigned short)((unsigned short)p[0] | ((unsigned short)p[1] << 8));
}

static unsigned int le32(const unsigned char* p) {
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8) |
           ((unsigned int)p[2] << 16) | ((unsigned int)p[3] << 24);
}

static void put16le(unsigned char* p, unsigned short value) {
    p[0] = (unsigned char)value;
    p[1] = (unsigned char)(value >> 8);
}

static void put32le(unsigned char* p, unsigned int value) {
    p[0] = (unsigned char)value;
    p[1] = (unsigned char)(value >> 8);
    p[2] = (unsigned char)(value >> 16);
    p[3] = (unsigned char)(value >> 24);
}

static void put16be(unsigned char* p, unsigned short value) {
    p[0] = (unsigned char)(value >> 8);
    p[1] = (unsigned char)value;
}

static void put32be(unsigned char* p, unsigned int value) {
    p[0] = (unsigned char)(value >> 24);
    p[1] = (unsigned char)(value >> 16);
    p[2] = (unsigned char)(value >> 8);
    p[3] = (unsigned char)value;
}

static void cache_writeback_invalidate(void) {
    __asm__ volatile("wbinvd" : : : "memory");
}

static void delay_approx_ms(unsigned int ms) {
    unsigned int i;
    while (ms-- != 0u) {
        for (i = 0; i < 4096u; ++i) {
            (void)inb(0x0080u);
        }
    }
}

static unsigned int align_up_u32(unsigned int value, unsigned int align) {
    if (align == 0u) {
        return value;
    }
    return (value + align - 1u) & ~(align - 1u);
}

#define PCI_MAX_BUS 16u
#define PCI_MAX_BRIDGES 16u
#define PCI_ROOT_IO_BASE 0x2000u
#define PCI_ROOT_MEM_MIN_BASE 0x04000000u
#define PCI_ROOT_MEM_ALIGN 0x01000000u
#define PCI_BRIDGE_IO_ALIGN 0x00001000u
#define PCI_BRIDGE_MEM_ALIGN 0x00100000u

struct pci_resource_req {
    unsigned int io;
    unsigned int mem;
    unsigned int pref;
    unsigned char has_vga;
};

struct pci_allocator {
    unsigned int io;
    unsigned int mem;
    unsigned int pref;
};

struct pci_bar_info {
    unsigned char valid;
    unsigned char is_io;
    unsigned char is_pref;
    unsigned char is_64;
    unsigned char index;
    unsigned int size;
    unsigned int flags;
};

struct pci_bridge_info {
    unsigned char valid;
    unsigned char bus;
    unsigned char dev;
    unsigned char fn;
    unsigned char secondary;
    unsigned char subordinate;
    struct pci_resource_req req;
};

static struct pci_bridge_info pci_bridges[PCI_MAX_BRIDGES];
static unsigned char pci_bridge_count = 0;

static void pci_req_clear(struct pci_resource_req* req) {
    req->io = 0u;
    req->mem = 0u;
    req->pref = 0u;
    req->has_vga = 0u;
}

static void pci_req_add(unsigned int* cursor, unsigned int size,
                        unsigned int align) {
    if (size == 0u) {
        return;
    }
    *cursor = align_up_u32(*cursor, align) + size;
}

static unsigned int pci_window_size(unsigned int size, unsigned int align) {
    if (size == 0u) {
        return 0u;
    }
    return align_up_u32(size, align);
}

static void pci_print_bdf(unsigned char bus, unsigned char dev,
                          unsigned char fn) {
    serial_write_hex8(bus);
    serial_write_char(':');
    serial_write_hex8(dev);
    serial_write_char('.');
    serial_write_hex8(fn);
}

static void pci_set_pirq_routes(unsigned char bus, unsigned char dev,
                                unsigned char fn, unsigned short device_id) {
    unsigned char reg;
    unsigned char is_pirq_router = 0u;

    if (bus == 0u && fn == 0u && dev == 1u && device_id == 0x7000u) {
        is_pirq_router = 1u;
    }
    if (bus == 0u && fn == 0u && dev == 7u && device_id == 0x7110u) {
        is_pirq_router = 1u;
    }
    if (!is_pirq_router) {
        return;
    }

    for (reg = 0x60u; reg <= 0x63u; ++reg) {
        pci_write8(bus, dev, fn, reg, 0x0bu);
    }
    outb(0x04d0u, inb(0x04d0u));
    outb(0x04d1u, (unsigned char)(inb(0x04d1u) | 0x08u));
    serial_write_string("  PIRQ A-D -> IRQ11\r\n");
}

static void pci_assign_interrupt_line(unsigned char bus, unsigned char dev,
                                      unsigned char fn) {
    unsigned char pin = pci_read8(bus, dev, fn, 0x3du);
    if (pin == 0u || pin > 4u) {
        return;
    }
    pci_write8(bus, dev, fn, 0x3cu, 11u);
    serial_write_string("  irq pin=");
    serial_write_char((char)('A' + pin - 1u));
    serial_write_string(" line=0b\r\n");
}

static unsigned char pci_bar_count(unsigned char header_type) {
    unsigned char type = header_type & 0x7fu;
    if (type == 0x00u) {
        return 6u;
    }
    if (type == 0x01u) {
        return 2u;
    }
    return 0u;
}

static int pci_probe_bar(unsigned char bus, unsigned char dev, unsigned char fn,
                         unsigned char bar_index, unsigned char bar_count,
                         struct pci_bar_info* info) {
    unsigned char reg = (unsigned char)(0x10u + bar_index * 4u);
    unsigned int orig;
    unsigned int mask;
    unsigned int orig_hi = 0u;
    unsigned short command;

    info->valid = 0u;
    info->is_io = 0u;
    info->is_pref = 0u;
    info->is_64 = 0u;
    info->index = bar_index;
    info->size = 0u;
    info->flags = 0u;

    orig = pci_read32(bus, dev, fn, reg);
    if (orig == 0xffffffffu) {
        return 0;
    }

    command = pci_read16(bus, dev, fn, 0x04u);
    pci_write16(bus, dev, fn, 0x04u, (unsigned short)(command & ~0x0003u));

    if ((orig & 0x00000001u) == 0u &&
        (orig & 0x00000006u) == 0x00000004u &&
        (bar_index + 1u) < bar_count) {
        orig_hi = pci_read32(bus, dev, fn, (unsigned char)(reg + 4u));
        pci_write32(bus, dev, fn, (unsigned char)(reg + 4u), 0xffffffffu);
    }

    pci_write32(bus, dev, fn, reg, 0xffffffffu);
    mask = pci_read32(bus, dev, fn, reg);
    pci_write32(bus, dev, fn, reg, orig);
    if ((orig & 0x00000001u) == 0u &&
        (orig & 0x00000006u) == 0x00000004u &&
        (bar_index + 1u) < bar_count) {
        pci_write32(bus, dev, fn, (unsigned char)(reg + 4u), orig_hi);
    }
    pci_write16(bus, dev, fn, 0x04u, command);

    if (mask == 0u || mask == 0xffffffffu) {
        return 0;
    }

    if ((orig & 0x00000001u) != 0u) {
        info->size = (~(mask & 0xfffffffcu)) + 1u;
        if (info->size == 0u || info->size > 0x00010000u) {
            return 0;
        }
        info->valid = 1u;
        info->is_io = 1u;
        info->flags = orig & 0x00000003u;
        return 1;
    }

    info->size = (~(mask & 0xfffffff0u)) + 1u;
    if (info->size == 0u) {
        return 0;
    }
    info->valid = 1u;
    info->is_pref = (orig & 0x00000008u) != 0u ? 1u : 0u;
    info->is_64 = (orig & 0x00000006u) == 0x00000004u ? 1u : 0u;
    info->flags = orig & 0x0000000fu;
    return 1;
}

static void pci_measure_bars(unsigned char bus, unsigned char dev,
                             unsigned char fn, unsigned char header_type,
                             struct pci_resource_req* req) {
    unsigned char bar_index;
    unsigned char count = pci_bar_count(header_type);

    for (bar_index = 0u; bar_index < count; ++bar_index) {
        struct pci_bar_info bar;
        if (!pci_probe_bar(bus, dev, fn, bar_index, count, &bar)) {
            continue;
        }
        serial_write_string("  BAR");
        serial_write_hex8(bar_index);
        if (bar.is_io) {
            pci_req_add(&req->io, bar.size, bar.size);
            serial_write_string(" req io sz=");
            serial_write_hex32(bar.size);
        } else if (bar.is_pref) {
            pci_req_add(&req->pref, bar.size, bar.size);
            serial_write_string(" req pref sz=");
            serial_write_hex32(bar.size);
        } else {
            pci_req_add(&req->mem, bar.size, bar.size);
            serial_write_string(" req mem sz=");
            serial_write_hex32(bar.size);
        }
        serial_write_string("\r\n");
        if (bar.is_64) {
            ++bar_index;
        }
    }
}

static struct pci_bridge_info* pci_find_bridge_info(unsigned char bus,
                                                    unsigned char dev,
                                                    unsigned char fn) {
    unsigned char i;
    for (i = 0u; i < pci_bridge_count; ++i) {
        if (pci_bridges[i].valid && pci_bridges[i].bus == bus &&
            pci_bridges[i].dev == dev && pci_bridges[i].fn == fn) {
            return &pci_bridges[i];
        }
    }
    return 0;
}

static void pci_bridge_disable_windows(unsigned char bus, unsigned char dev,
                                       unsigned char fn) {
    pci_write16(bus, dev, fn, 0x1cu, 0x00f0u);
    pci_write32(bus, dev, fn, 0x20u, 0x0000fff0u);
    pci_write32(bus, dev, fn, 0x24u, 0x0000fff0u);
    pci_write32(bus, dev, fn, 0x28u, 0x00000000u);
    pci_write32(bus, dev, fn, 0x2cu, 0x00000000u);
    pci_write32(bus, dev, fn, 0x30u, 0x00000000u);
}

static void pci_bridge_set_io_window(unsigned char bus, unsigned char dev,
                                     unsigned char fn, unsigned int base,
                                     unsigned int size) {
    unsigned int limit;
    unsigned char base_reg;
    unsigned char limit_reg;

    if (size == 0u) {
        pci_write16(bus, dev, fn, 0x1cu, 0x00f0u);
        pci_write32(bus, dev, fn, 0x30u, 0x00000000u);
        return;
    }
    limit = base + size - 1u;
    base_reg = (unsigned char)((base >> 8) & 0xf0u);
    limit_reg = (unsigned char)((limit >> 8) & 0xf0u);
    pci_write16(bus, dev, fn, 0x1cu,
                (unsigned short)base_reg |
                    (unsigned short)((unsigned short)limit_reg << 8));
    pci_write16(bus, dev, fn, 0x30u, (unsigned short)(base >> 16));
    pci_write16(bus, dev, fn, 0x32u, (unsigned short)(limit >> 16));
}

static void pci_bridge_set_mem_window(unsigned char bus, unsigned char dev,
                                      unsigned char fn, unsigned char reg,
                                      unsigned int base, unsigned int size) {
    unsigned int limit;
    unsigned int base_reg;
    unsigned int limit_reg;

    if (size == 0u) {
        pci_write32(bus, dev, fn, reg, 0x0000fff0u);
        if (reg == 0x24u) {
            pci_write32(bus, dev, fn, 0x28u, 0x00000000u);
            pci_write32(bus, dev, fn, 0x2cu, 0x00000000u);
        }
        return;
    }
    limit = base + size - 1u;
    base_reg = (base >> 16) & 0xfff0u;
    limit_reg = (limit >> 16) & 0xfff0u;
    pci_write32(bus, dev, fn, reg, base_reg | (limit_reg << 16));
    if (reg == 0x24u) {
        pci_write32(bus, dev, fn, 0x28u, 0x00000000u);
        pci_write32(bus, dev, fn, 0x2cu, 0x00000000u);
    }
}

static void pci_note_storage_device(unsigned char bus, unsigned char dev,
                                    unsigned char fn,
                                    unsigned int class_code) {
    if (((class_code >> 16) & 0xffu) == 0x01u &&
        ((class_code >> 8) & 0xffu) == 0x01u) {
        storage_ide_bdf.bus = bus;
        storage_ide_bdf.dev = dev;
        storage_ide_bdf.fn = fn;
        storage_ide_found = 1u;
    }
    if (class_code == 0x0c0300u) {
        storage_uhci_bdf.bus = bus;
        storage_uhci_bdf.dev = dev;
        storage_uhci_bdf.fn = fn;
        storage_uhci_found = 1u;
    }
}

static void pci_print_device(unsigned char bus, unsigned char dev,
                             unsigned char fn, unsigned short vendor,
                             unsigned short device_id,
                             unsigned int class_code) {
    serial_write_string("PCI ");
    pci_print_bdf(bus, dev, fn);
    serial_write_char(' ');
    serial_write_hex16(vendor);
    serial_write_char(':');
    serial_write_hex16(device_id);
    serial_write_string(" cls=");
    serial_write_hex32(class_code);
    serial_write_string(" cmd=");
    serial_write_hex16(pci_read16(bus, dev, fn, 0x04u));
    serial_write_string("\r\n");
}

static struct pci_resource_req pci_measure_bus(unsigned char bus,
                                               unsigned char* next_bus) {
    struct pci_resource_req req;
    unsigned char dev;
    unsigned char fn;

    pci_req_clear(&req);

    for (dev = 0u; dev < 32u; ++dev) {
        unsigned short vendor0 = pci_read16(bus, dev, 0, 0x00u);
        unsigned char header0;
        unsigned char fn_count;

        if (vendor0 == 0xffffu) {
            continue;
        }
        header0 = pci_read8(bus, dev, 0, 0x0eu);
        fn_count = (header0 & 0x80u) != 0u ? 8u : 1u;
        for (fn = 0u; fn < fn_count; ++fn) {
            unsigned int id = pci_read32(bus, dev, fn, 0x00u);
            unsigned int revclass;
            unsigned int class_code;
            unsigned char header;
            unsigned char header_kind;
            unsigned short vendor = (unsigned short)id;
            unsigned short device_id = (unsigned short)(id >> 16);

            if (vendor == 0xffffu) {
                continue;
            }
            revclass = pci_read32(bus, dev, fn, 0x08u);
            class_code = revclass >> 8;
            header = pci_read8(bus, dev, fn, 0x0eu);
            header_kind = header & 0x7fu;

            pci_print_device(bus, dev, fn, vendor, device_id, class_code);
            pci_set_pirq_routes(bus, dev, fn, device_id);
            pci_assign_interrupt_line(bus, dev, fn);
            pci_note_storage_device(bus, dev, fn, class_code);

            if (header_kind == 0x01u) {
                struct pci_bridge_info* bridge;
                struct pci_resource_req child;
                unsigned char secondary;
                unsigned char subordinate;

                pci_measure_bars(bus, dev, fn, header, &req);
                if (pci_bridge_count >= PCI_MAX_BRIDGES ||
                    *next_bus >= PCI_MAX_BUS) {
                    serial_write_string("  bridge skipped: no bus slot\r\n");
                    continue;
                }

                bridge = &pci_bridges[pci_bridge_count++];
                bridge->valid = 1u;
                bridge->bus = bus;
                bridge->dev = dev;
                bridge->fn = fn;
                secondary = *next_bus;
                *next_bus = (unsigned char)(*next_bus + 1u);
                bridge->secondary = secondary;

                pci_write8(bus, dev, fn, 0x18u, bus);
                pci_write8(bus, dev, fn, 0x19u, secondary);
                pci_write8(bus, dev, fn, 0x1au, 0xffu);
                pci_bridge_disable_windows(bus, dev, fn);

                child = pci_measure_bus(secondary, next_bus);
                subordinate = (unsigned char)(*next_bus - 1u);
                bridge->subordinate = subordinate;
                pci_write8(bus, dev, fn, 0x1au, subordinate);

                bridge->req.io =
                    pci_window_size(child.io, PCI_BRIDGE_IO_ALIGN);
                bridge->req.mem =
                    pci_window_size(child.mem, PCI_BRIDGE_MEM_ALIGN);
                bridge->req.pref =
                    pci_window_size(child.pref, PCI_BRIDGE_MEM_ALIGN);
                bridge->req.has_vga = child.has_vga;

                pci_req_add(&req.io, bridge->req.io, PCI_BRIDGE_IO_ALIGN);
                pci_req_add(&req.mem, bridge->req.mem, PCI_BRIDGE_MEM_ALIGN);
                pci_req_add(&req.pref, bridge->req.pref, PCI_BRIDGE_MEM_ALIGN);
                if (child.has_vga) {
                    req.has_vga = 1u;
                }

                serial_write_string("  bridge bus=");
                serial_write_hex8(secondary);
                serial_write_char('-');
                serial_write_hex8(subordinate);
                serial_write_string(" io=");
                serial_write_hex32(bridge->req.io);
                serial_write_string(" mem=");
                serial_write_hex32(bridge->req.mem);
                serial_write_string(" pref=");
                serial_write_hex32(bridge->req.pref);
                serial_write_string("\r\n");
            } else if (header_kind == 0x00u) {
                pci_measure_bars(bus, dev, fn, header, &req);
                if (((class_code >> 16) & 0xffu) == 0x03u) {
                    req.has_vga = 1u;
                }
            }
        }
    }
    return req;
}

static void pci_assign_bars(unsigned char bus, unsigned char dev,
                            unsigned char fn, unsigned char header_type,
                            unsigned int class_code,
                            struct pci_allocator* alloc) {
    unsigned char bar_index;
    unsigned char count = pci_bar_count(header_type);
    unsigned short command = pci_read16(bus, dev, fn, 0x04u);
    unsigned short new_command = command;
    unsigned char assigned_io = 0u;
    unsigned char assigned_mem = 0u;

    if (count == 0u) {
        return;
    }

    pci_write16(bus, dev, fn, 0x04u, (unsigned short)(command & ~0x0003u));
    for (bar_index = 0u; bar_index < count; ++bar_index) {
        struct pci_bar_info bar;
        unsigned int base;
        unsigned char reg = (unsigned char)(0x10u + bar_index * 4u);

        if (!pci_probe_bar(bus, dev, fn, bar_index, count, &bar)) {
            continue;
        }

        if (bar.is_io) {
            base = align_up_u32(alloc->io, bar.size);
            alloc->io = base + bar.size;
            pci_write32(bus, dev, fn, reg, base | 0x00000001u);
            assigned_io = 1u;
            serial_write_string("  BAR");
            serial_write_hex8(bar_index);
            serial_write_string(" io=");
            serial_write_hex16((unsigned short)base);
            serial_write_string(" sz=");
            serial_write_hex32(bar.size);
            serial_write_string("\r\n");
        } else if (bar.is_pref) {
            base = align_up_u32(alloc->pref, bar.size);
            alloc->pref = base + bar.size;
            pci_write32(bus, dev, fn, reg, base | bar.flags);
            if (bar.is_64 && (bar_index + 1u) < count) {
                pci_write32(bus, dev, fn, (unsigned char)(reg + 4u), 0u);
            }
            assigned_mem = 1u;
            serial_write_string("  BAR");
            serial_write_hex8(bar_index);
            serial_write_string(" pref=");
            serial_write_hex32(base);
            serial_write_string(" sz=");
            serial_write_hex32(bar.size);
            serial_write_string("\r\n");
        } else {
            base = align_up_u32(alloc->mem, bar.size);
            alloc->mem = base + bar.size;
            pci_write32(bus, dev, fn, reg, base | bar.flags);
            if (bar.is_64 && (bar_index + 1u) < count) {
                pci_write32(bus, dev, fn, (unsigned char)(reg + 4u), 0u);
            }
            assigned_mem = 1u;
            serial_write_string("  BAR");
            serial_write_hex8(bar_index);
            serial_write_string(" mem=");
            serial_write_hex32(base);
            serial_write_string(" sz=");
            serial_write_hex32(bar.size);
            serial_write_string("\r\n");
        }
        if (bar.is_64) {
            ++bar_index;
        }
    }

    if (assigned_io) {
        new_command |= 0x0001u;
    }
    if (assigned_mem) {
        new_command |= 0x0002u;
    }
    if (assigned_io || assigned_mem || ((class_code >> 16) == 0x01u) ||
        ((class_code >> 16) == 0x02u) || ((class_code >> 16) == 0x03u) ||
        ((class_code >> 16) == 0x0cu)) {
        new_command |= 0x0004u;
    }
    pci_write16(bus, dev, fn, 0x04u, new_command);
    if (new_command != command) {
        serial_write_string("  cmd ");
        serial_write_hex16(command);
        serial_write_string("->");
        serial_write_hex16(new_command);
        serial_write_string("\r\n");
    }
}

static void pci_assign_bus(unsigned char bus, struct pci_allocator* alloc) {
    unsigned char dev;
    unsigned char fn;

    for (dev = 0u; dev < 32u; ++dev) {
        unsigned short vendor0 = pci_read16(bus, dev, 0, 0x00u);
        unsigned char header0;
        unsigned char fn_count;

        if (vendor0 == 0xffffu) {
            continue;
        }
        header0 = pci_read8(bus, dev, 0, 0x0eu);
        fn_count = (header0 & 0x80u) != 0u ? 8u : 1u;
        for (fn = 0u; fn < fn_count; ++fn) {
            unsigned int id = pci_read32(bus, dev, fn, 0x00u);
            unsigned int class_code;
            unsigned char header;
            unsigned char header_kind;
            unsigned short vendor = (unsigned short)id;

            if (vendor == 0xffffu) {
                continue;
            }

            class_code = pci_read32(bus, dev, fn, 0x08u) >> 8;
            header = pci_read8(bus, dev, fn, 0x0eu);
            header_kind = header & 0x7fu;

            pci_assign_bars(bus, dev, fn, header, class_code, alloc);
            if (header_kind == 0x01u) {
                struct pci_bridge_info* bridge =
                    pci_find_bridge_info(bus, dev, fn);
                struct pci_allocator child_alloc;
                unsigned int io_base = 0u;
                unsigned int mem_base = 0u;
                unsigned int pref_base = 0u;
                unsigned short command;
                unsigned short bridge_control;

                if (bridge == 0) {
                    continue;
                }

                if (bridge->req.io != 0u) {
                    io_base = align_up_u32(alloc->io, PCI_BRIDGE_IO_ALIGN);
                    alloc->io = io_base + bridge->req.io;
                }
                if (bridge->req.mem != 0u) {
                    mem_base = align_up_u32(alloc->mem, PCI_BRIDGE_MEM_ALIGN);
                    alloc->mem = mem_base + bridge->req.mem;
                }
                if (bridge->req.pref != 0u) {
                    pref_base =
                        align_up_u32(alloc->pref, PCI_BRIDGE_MEM_ALIGN);
                    alloc->pref = pref_base + bridge->req.pref;
                }

                pci_bridge_set_io_window(bus, dev, fn, io_base,
                                         bridge->req.io);
                pci_bridge_set_mem_window(bus, dev, fn, 0x20u, mem_base,
                                          bridge->req.mem);
                pci_bridge_set_mem_window(bus, dev, fn, 0x24u, pref_base,
                                          bridge->req.pref);

                command = pci_read16(bus, dev, fn, 0x04u);
                command = (unsigned short)(command | 0x0004u);
                if (bridge->req.io != 0u) {
                    command = (unsigned short)(command | 0x0001u);
                }
                if (bridge->req.mem != 0u || bridge->req.pref != 0u) {
                    command = (unsigned short)(command | 0x0002u);
                }
                pci_write16(bus, dev, fn, 0x04u, command);

                bridge_control = pci_read16(bus, dev, fn, 0x3eu);
                if (bridge->req.has_vga) {
                    bridge_control =
                        (unsigned short)(bridge_control | 0x000cu);
                }
                pci_write16(bus, dev, fn, 0x3eu, bridge_control);

                serial_write_string("  window bus=");
                serial_write_hex8(bridge->secondary);
                serial_write_string(" io=");
                serial_write_hex32(io_base);
                serial_write_string("+");
                serial_write_hex32(bridge->req.io);
                serial_write_string(" mem=");
                serial_write_hex32(mem_base);
                serial_write_string("+");
                serial_write_hex32(bridge->req.mem);
                serial_write_string(" pref=");
                serial_write_hex32(pref_base);
                serial_write_string("+");
                serial_write_hex32(bridge->req.pref);
                serial_write_string(" ctl=");
                serial_write_hex16(bridge_control);
                serial_write_string("\r\n");

                child_alloc.io = io_base;
                child_alloc.mem = mem_base;
                child_alloc.pref = pref_base;
                pci_assign_bus(bridge->secondary, &child_alloc);
            }
        }
    }
}

static void pci_enumerate_and_assign(unsigned int total_bytes) {
    struct pci_resource_req root_req;
    struct pci_allocator root_alloc;
    unsigned char next_bus = 1u;
    unsigned char i;
    unsigned int mem_base;

    outb(0x80, POST_PCI_PROBE_START);
    storage_ide_found = 0;
    storage_uhci_found = 0;
    pci_bridge_count = 0u;
    for (i = 0u; i < PCI_MAX_BRIDGES; ++i) {
        pci_bridges[i].valid = 0u;
    }

    serial_write_string("PCI enum...\r\n");
    root_req = pci_measure_bus(0u, &next_bus);

    mem_base = align_up_u32(total_bytes, PCI_ROOT_MEM_ALIGN);
    if (mem_base < PCI_ROOT_MEM_MIN_BASE) {
        mem_base = PCI_ROOT_MEM_MIN_BASE;
    }

    root_alloc.io = PCI_ROOT_IO_BASE;
    root_alloc.mem = mem_base;
    root_alloc.pref =
        align_up_u32(mem_base + pci_window_size(root_req.mem,
                                                PCI_ROOT_MEM_ALIGN),
                     PCI_ROOT_MEM_ALIGN);

    serial_write_string("PCI root req io=");
    serial_write_hex32(root_req.io);
    serial_write_string(" mem=");
    serial_write_hex32(root_req.mem);
    serial_write_string(" pref=");
    serial_write_hex32(root_req.pref);
    serial_write_string(" base mem=");
    serial_write_hex32(root_alloc.mem);
    serial_write_string(" pref=");
    serial_write_hex32(root_alloc.pref);
    serial_write_string("\r\n");

    pci_assign_bus(0u, &root_alloc);
    pci_next_io = root_alloc.io;
    pci_next_mem = root_alloc.mem;
}

#define IDE_STATUS_BSY 0x80u
#define IDE_STATUS_DRDY 0x40u
#define IDE_STATUS_DRQ 0x08u
#define IDE_STATUS_ERR 0x01u
#define IDE_CMD_READ_SECTORS 0x20u
#define IDE_CMD_READ_DMA_EXT 0x25u
#define IDE_CMD_READ_DMA 0xc8u
#define IDE_CMD_SET_FEATURES 0xefu
#define IDE_FEATURE_SET_TRANSFER_MODE 0x03u
#define IDE_XFER_MWDMA0 0x20u
#define IDE_XFER_UDMA0 0x40u
#define IDE_BM_CMD_START 0x01u
#define IDE_BM_CMD_READ 0x08u
#define IDE_BM_STATUS_ACTIVE 0x01u
#define IDE_BM_STATUS_ERROR 0x02u
#define IDE_BM_STATUS_INTR 0x04u
#define IDE_BM_STATUS_CLEAR (IDE_BM_STATUS_ERROR | IDE_BM_STATUS_INTR)

static void ide_400ns_delay(unsigned short ctrl) {
    (void)inb(ctrl);
    (void)inb(ctrl);
    (void)inb(ctrl);
    (void)inb(ctrl);
}

static int ide_wait_not_busy(unsigned short io) {
    unsigned int timeout = 2000000u;
    unsigned char st;
    do {
        st = inb((unsigned short)(io + 7u));
        if ((st & IDE_STATUS_BSY) == 0u) {
            return 0;
        }
    } while (--timeout != 0u);
    return -1;
}

static int ide_wait_not_busy_or_err(unsigned short io) {
    unsigned int timeout = 2000000u;
    unsigned char st;
    do {
        st = inb((unsigned short)(io + 7u));
        if ((st & IDE_STATUS_ERR) != 0u) {
            return -1;
        }
        if ((st & IDE_STATUS_BSY) == 0u) {
            return 0;
        }
    } while (--timeout != 0u);
    return -1;
}

static int ide_wait_drq(unsigned short io) {
    unsigned int timeout = 2000000u;
    unsigned char st;
    do {
        st = inb((unsigned short)(io + 7u));
        if ((st & IDE_STATUS_ERR) != 0u) {
            return -1;
        }
        if ((st & IDE_STATUS_BSY) == 0u && (st & IDE_STATUS_DRQ) != 0u) {
            return 0;
        }
    } while (--timeout != 0u);
    return -1;
}

static int ide_set_transfer_mode(unsigned short io, unsigned short ctrl,
                                 unsigned char drive, unsigned char mode) {
    outb(ctrl, 0x02u);
    outb((unsigned short)(io + 6u), (unsigned char)(0xe0u | (drive << 4)));
    ide_400ns_delay(ctrl);
    if (ide_wait_not_busy(io) != 0) {
        return -1;
    }
    outb((unsigned short)(io + 1u), IDE_FEATURE_SET_TRANSFER_MODE);
    outb((unsigned short)(io + 2u), mode);
    outb((unsigned short)(io + 7u), IDE_CMD_SET_FEATURES);
    return ide_wait_not_busy_or_err(io);
}

static unsigned char ide_best_mwdma_mode(const unsigned short* id) {
    unsigned short modes;
    if ((id[49] & 0x0100u) == 0u) {
        return 0xffu;
    }
    modes = id[63] & 0x0007u;
    if ((modes & 0x0004u) != 0u) {
        return 2u;
    }
    if ((modes & 0x0002u) != 0u) {
        return 1u;
    }
    if ((modes & 0x0001u) != 0u) {
        return 0u;
    }
    return 0xffu;
}

static unsigned char ide_best_udma_mode(const unsigned short* id) {
    unsigned short modes;
    if ((id[53] & 0x0004u) == 0u) {
        return 0xffu;
    }
    modes = id[88] & 0x0007u;
    if ((modes & 0x0004u) != 0u) {
        return 2u;
    }
    if ((modes & 0x0002u) != 0u) {
        return 1u;
    }
    if ((modes & 0x0001u) != 0u) {
        return 0u;
    }
    return 0xffu;
}

static unsigned char ide_supports_lba48(const unsigned short* id) {
    if ((id[83] & 0xc000u) != 0x4000u) {
        return 0u;
    }
    return (id[83] & 0x0400u) != 0u ? 1u : 0u;
}

static void ide_configure_piix4_udma(unsigned char channel,
                                     unsigned char drive,
                                     unsigned char mode) {
    unsigned char enable_bit = (unsigned char)(channel * 2u + drive);
    unsigned char shift = (unsigned char)(channel * 8u + drive * 4u);
    unsigned short mask = (unsigned short)(0x0003u << shift);

    storage_ide_udmactl =
        (unsigned char)(storage_ide_udmactl | (unsigned char)(1u << enable_bit));
    storage_ide_udmatim =
        (unsigned short)((storage_ide_udmatim & (unsigned short)~mask) |
                         ((unsigned short)(mode & 0x03u) << shift));
    pci_write16(storage_ide_bdf.bus, storage_ide_bdf.dev, storage_ide_bdf.fn,
                0x4au, storage_ide_udmatim);
    pci_write8(storage_ide_bdf.bus, storage_ide_bdf.dev, storage_ide_bdf.fn,
               0x48u, storage_ide_udmactl);
}

static int ide_identify(unsigned short io, unsigned short ctrl,
                        unsigned char drive, unsigned short* words) {
    unsigned int i;
    unsigned char st;

    outb(ctrl, 0x02u);
    outb((unsigned short)(io + 6u), (unsigned char)(0xa0u | (drive << 4)));
    ide_400ns_delay(ctrl);
    if (ide_wait_not_busy(io) != 0) {
        return -1;
    }
    outb((unsigned short)(io + 2u), 0u);
    outb((unsigned short)(io + 3u), 0u);
    outb((unsigned short)(io + 4u), 0u);
    outb((unsigned short)(io + 5u), 0u);
    outb((unsigned short)(io + 7u), 0xecu);
    st = inb((unsigned short)(io + 7u));
    if (st == 0u || st == 0xffu) {
        return -1;
    }
    if (ide_wait_not_busy(io) != 0) {
        return -1;
    }
    if (inb((unsigned short)(io + 4u)) != 0u ||
        inb((unsigned short)(io + 5u)) != 0u) {
        return -1;
    }
    if (ide_wait_drq(io) != 0) {
        return -1;
    }
    for (i = 0; i < 256u; ++i) {
        words[i] = inw(io);
    }
    return 0;
}

static int ide_read_lba28(unsigned short io, unsigned short ctrl,
                          unsigned char drive, unsigned int lba,
                          unsigned int count, unsigned char* dst) {
    unsigned int sector;

    if (count == 0u || count > 256u) {
        return -1;
    }

    outb(ctrl, 0x02u);
    outb((unsigned short)(io + 6u),
         (unsigned char)(0xe0u | (drive << 4) | ((lba >> 24) & 0x0fu)));
    ide_400ns_delay(ctrl);
    if (ide_wait_not_busy(io) != 0) {
        return -1;
    }
    outb((unsigned short)(io + 2u), (unsigned char)count);
    outb((unsigned short)(io + 3u), (unsigned char)lba);
    outb((unsigned short)(io + 4u), (unsigned char)(lba >> 8));
    outb((unsigned short)(io + 5u), (unsigned char)(lba >> 16));
    outb((unsigned short)(io + 7u), IDE_CMD_READ_SECTORS);

    for (sector = 0u; sector < count; ++sector) {
        unsigned int words = 256u;
        unsigned short* out = (unsigned short*)(dst + sector * 512u);
        if (ide_wait_drq(io) != 0) {
            return -1;
        }
        __asm__ volatile("cld\n\trep insw"
                         : "+D"(out), "+c"(words)
                         : "d"(io)
                         : "memory");
        ide_400ns_delay(ctrl);
    }
    return 0;
}

struct ide_prd {
    unsigned int base;
    unsigned int count_eot;
};

static int ide_build_prd(unsigned int dest, unsigned int bytes) {
    struct ide_prd* prd = (struct ide_prd*)IDE_PRD_LINEAR;
    unsigned int index = 0u;

    if (bytes == 0u) {
        return -1;
    }
    while (bytes != 0u) {
        unsigned int boundary = 0x10000u - (dest & 0xffffu);
        unsigned int chunk = bytes;
        unsigned int count_field;

        if (chunk > boundary) {
            chunk = boundary;
        }
        if (chunk > 0x10000u) {
            chunk = 0x10000u;
        }
        if (chunk == 0u || index >= IDE_MAX_PRD) {
            return -1;
        }

        count_field = (chunk == 0x10000u) ? 0u : chunk;
        prd[index].base = dest;
        prd[index].count_eot = count_field;
        dest += chunk;
        bytes -= chunk;
        ++index;
    }
    prd[index - 1u].count_eot |= 0x80000000u;
    return 0;
}

static int ide_read_dma_lba28(unsigned short io, unsigned short ctrl,
                              unsigned short bmio, unsigned char drive,
                              unsigned int lba, unsigned int count,
                              unsigned char* dst) {
    unsigned int timeout = 4000000u;
    unsigned char bmst;
    unsigned char st;

    if (bmio == 0u || count == 0u || count > 256u ||
        ide_build_prd((unsigned int)dst, count * 512u) != 0) {
        return -1;
    }

    outb(bmio, 0x00u);
    outb((unsigned short)(bmio + 2u), IDE_BM_STATUS_CLEAR);
    outl((unsigned short)(bmio + 4u), IDE_PRD_LINEAR);

    outb(ctrl, 0x02u);
    outb((unsigned short)(io + 6u),
         (unsigned char)(0xe0u | (drive << 4) | ((lba >> 24) & 0x0fu)));
    ide_400ns_delay(ctrl);
    if (ide_wait_not_busy(io) != 0) {
        return -1;
    }
    outb((unsigned short)(io + 2u), (unsigned char)count);
    outb((unsigned short)(io + 3u), (unsigned char)lba);
    outb((unsigned short)(io + 4u), (unsigned char)(lba >> 8));
    outb((unsigned short)(io + 5u), (unsigned char)(lba >> 16));
    outb((unsigned short)(io + 7u), IDE_CMD_READ_DMA);
    outb(bmio, IDE_BM_CMD_READ | IDE_BM_CMD_START);

    do {
        bmst = inb((unsigned short)(bmio + 2u));
        if ((bmst & IDE_BM_STATUS_ERROR) != 0u) {
            break;
        }
        if ((bmst & IDE_BM_STATUS_ACTIVE) == 0u) {
            break;
        }
    } while (--timeout != 0u);

    outb(bmio, IDE_BM_CMD_READ);
    st = inb((unsigned short)(io + 7u));
    bmst = inb((unsigned short)(bmio + 2u));
    outb((unsigned short)(bmio + 2u), IDE_BM_STATUS_CLEAR);

    if (timeout == 0u || (bmst & IDE_BM_STATUS_ERROR) != 0u ||
        (st & (IDE_STATUS_BSY | IDE_STATUS_ERR)) != 0u) {
        return -1;
    }
    return 0;
}

static int ide_read_dma_lba48(unsigned short io, unsigned short ctrl,
                              unsigned short bmio, unsigned char drive,
                              unsigned int lba, unsigned int count,
                              unsigned char* dst) {
    unsigned int timeout = 4000000u;
    unsigned char bmst;
    unsigned char st;

    if (bmio == 0u || count == 0u || count > 2048u ||
        ide_build_prd((unsigned int)dst, count * 512u) != 0) {
        return -1;
    }

    outb(bmio, 0x00u);
    outb((unsigned short)(bmio + 2u), IDE_BM_STATUS_CLEAR);
    outl((unsigned short)(bmio + 4u), IDE_PRD_LINEAR);

    outb(ctrl, 0x02u);
    outb((unsigned short)(io + 6u), (unsigned char)(0x40u | (drive << 4)));
    ide_400ns_delay(ctrl);
    if (ide_wait_not_busy(io) != 0) {
        return -1;
    }

    outb((unsigned short)(io + 2u), (unsigned char)(count >> 8));
    outb((unsigned short)(io + 3u), (unsigned char)(lba >> 24));
    outb((unsigned short)(io + 4u), 0u);
    outb((unsigned short)(io + 5u), 0u);
    outb((unsigned short)(io + 2u), (unsigned char)count);
    outb((unsigned short)(io + 3u), (unsigned char)lba);
    outb((unsigned short)(io + 4u), (unsigned char)(lba >> 8));
    outb((unsigned short)(io + 5u), (unsigned char)(lba >> 16));
    outb((unsigned short)(io + 7u), IDE_CMD_READ_DMA_EXT);
    outb(bmio, IDE_BM_CMD_READ | IDE_BM_CMD_START);

    do {
        bmst = inb((unsigned short)(bmio + 2u));
        if ((bmst & IDE_BM_STATUS_ERROR) != 0u) {
            break;
        }
        if ((bmst & IDE_BM_STATUS_ACTIVE) == 0u) {
            break;
        }
    } while (--timeout != 0u);

    outb(bmio, IDE_BM_CMD_READ);
    st = inb((unsigned short)(io + 7u));
    bmst = inb((unsigned short)(bmio + 2u));
    outb((unsigned short)(bmio + 2u), IDE_BM_STATUS_CLEAR);

    if (timeout == 0u || (bmst & IDE_BM_STATUS_ERROR) != 0u ||
        (st & (IDE_STATUS_BSY | IDE_STATUS_ERR)) != 0u) {
        return -1;
    }
    return 0;
}

static void bios_hdd_set_geometry(unsigned int sectors) {
    unsigned int cylinders;

    bios_hdd_spt = 63u;
    bios_hdd_heads = (sectors > (1024u * 16u * 63u)) ? 255u : 16u;
    cylinders = sectors / ((unsigned int)bios_hdd_heads * bios_hdd_spt);
    if (cylinders == 0u) {
        cylinders = 1u;
    }
    if (cylinders > 1024u) {
        cylinders = 1024u;
    }
    bios_hdd_cylinders = (unsigned short)cylinders;
}

static void bios_hdd_save_candidate(struct bios_hdd_candidate* cand) {
    cand->present = bios_hdd_present;
    cand->kind = bios_hdd_kind;
    cand->has_mbr = bios_hdd_has_mbr;
    cand->io = bios_hdd_io;
    cand->ctrl = bios_hdd_ctrl;
    cand->bmio = bios_hdd_bmio;
    cand->drive = bios_hdd_drive;
    cand->dma_enabled = bios_hdd_dma_enabled;
    cand->lba48_dma_enabled = bios_hdd_lba48_dma_enabled;
    storage_memcpy(&cand->usb_dev, &bios_hdd_usb_dev, sizeof(cand->usb_dev));
    cand->total_sectors = bios_hdd_total_sectors;
    cand->heads = bios_hdd_heads;
    cand->spt = bios_hdd_spt;
    cand->cylinders = bios_hdd_cylinders;
}

static void bios_hdd_activate_candidate(const struct bios_hdd_candidate* cand) {
    if (cand->present == 0u) {
        bios_hdd_present = 0u;
        bios_hdd_kind = BIOS_HDD_KIND_NONE;
        return;
    }
    bios_hdd_present = cand->present;
    bios_hdd_kind = cand->kind;
    bios_hdd_has_mbr = cand->has_mbr;
    bios_hdd_io = cand->io;
    bios_hdd_ctrl = cand->ctrl;
    bios_hdd_bmio = cand->bmio;
    bios_hdd_drive = cand->drive;
    bios_hdd_dma_enabled = cand->dma_enabled;
    bios_hdd_lba48_dma_enabled = cand->lba48_dma_enabled;
    bios_hdd_dma_fallback_logged = 0u;
    bios_hdd_dma_ext_fallback_logged = 0u;
    storage_memcpy(&bios_hdd_usb_dev, &cand->usb_dev, sizeof(bios_hdd_usb_dev));
    bios_hdd_total_sectors = cand->total_sectors;
    bios_hdd_heads = cand->heads;
    bios_hdd_spt = cand->spt;
    bios_hdd_cylinders = cand->cylinders;
}

static void bios_hdd_register(unsigned short io, unsigned short ctrl,
                              unsigned short bmio, unsigned char drive,
                              unsigned int sectors, unsigned char dma_enabled,
                              unsigned char lba48_dma_enabled) {
    if (bios_hdd_ide_candidate.present != 0u) {
        return;
    }
    bios_hdd_present = 1u;
    bios_hdd_kind = BIOS_HDD_KIND_IDE;
    bios_hdd_has_mbr = 0u;
    bios_hdd_io = io;
    bios_hdd_ctrl = ctrl;
    bios_hdd_bmio = bmio;
    bios_hdd_drive = drive;
    bios_hdd_dma_enabled = dma_enabled;
    bios_hdd_lba48_dma_enabled = lba48_dma_enabled;
    bios_hdd_dma_fallback_logged = 0u;
    bios_hdd_dma_ext_fallback_logged = 0u;
    bios_hdd_total_sectors = sectors;
    bios_hdd_set_geometry(sectors);
    serial_write_string("BIOS HDD80 IDE sectors=");
    serial_write_hex32(sectors);
    serial_write_string(" dma=");
    serial_write_hex8(dma_enabled);
    serial_write_string(" lba48=");
    serial_write_hex8(lba48_dma_enabled);
    serial_write_string(" C/H/S=");
    serial_write_u32(bios_hdd_cylinders);
    serial_write_char('/');
    serial_write_u32(bios_hdd_heads);
    serial_write_char('/');
    serial_write_u32(bios_hdd_spt);
    serial_write_string("\r\n");
    bios_hdd_save_candidate(&bios_hdd_ide_candidate);
}

static void bios_hdd_register_usb(struct usb_dev* dev, unsigned int sectors,
                                  unsigned char has_mbr) {
    bios_hdd_present = 1u;
    bios_hdd_kind = BIOS_HDD_KIND_USB;
    bios_hdd_has_mbr = has_mbr;
    storage_memcpy(&bios_hdd_usb_dev, dev, sizeof(bios_hdd_usb_dev));
    bios_hdd_total_sectors = sectors;
    bios_hdd_set_geometry(sectors);
    serial_write_string("BIOS HDD80 USB sectors=");
    serial_write_hex32(sectors);
    serial_write_string(" mbr=");
    serial_write_hex8(has_mbr);
    serial_write_string(" C/H/S=");
    serial_write_u32(bios_hdd_cylinders);
    serial_write_char('/');
    serial_write_u32(bios_hdd_heads);
    serial_write_char('/');
    serial_write_u32(bios_hdd_spt);
    serial_write_string("\r\n");
    bios_hdd_save_candidate(&bios_hdd_usb_candidate);
}

unsigned char bios_hdd_is_present(void) { return bios_hdd_present; }

unsigned char bios_hdd_current_kind(void) { return bios_hdd_kind; }

unsigned char bios_hdd_select_kind(unsigned char kind) {
    const struct bios_hdd_candidate* cand = 0;

    if (kind == BIOS_HDD_KIND_IDE) {
        cand = &bios_hdd_ide_candidate;
    } else if (kind == BIOS_HDD_KIND_USB) {
        cand = &bios_hdd_usb_candidate;
    } else {
        return 0u;
    }
    if (cand->present == 0u) {
        return 0u;
    }
    bios_hdd_activate_candidate(cand);
    serial_write_string("HDD80 select kind=");
    serial_write_hex8(kind);
    serial_write_string("\r\n");
    return 1u;
}

void bios_hdd_get_dma_caps(unsigned char* dma_enabled,
                           unsigned char* lba48_dma_enabled) {
    if (dma_enabled != 0) {
        *dma_enabled = bios_hdd_dma_enabled;
    }
    if (lba48_dma_enabled != 0) {
        *lba48_dma_enabled = bios_hdd_lba48_dma_enabled;
    }
}

void bios_hdd_set_dma_caps(unsigned char dma_enabled,
                           unsigned char lba48_dma_enabled) {
    bios_hdd_dma_enabled = dma_enabled;
    bios_hdd_lba48_dma_enabled = lba48_dma_enabled;
}

void bios_hdd_get_geometry(struct bios_hdd_geometry* geometry) {
    geometry->total_sectors = bios_hdd_total_sectors;
    geometry->cylinders = bios_hdd_cylinders;
    geometry->heads = bios_hdd_heads;
    geometry->sectors_per_track = bios_hdd_spt;
}

static int usb_msd_read_lba_count(struct usb_dev* dev, unsigned int lba,
                                  unsigned int count, unsigned char* sector);

static unsigned int usb_msd_max_read_sectors(struct usb_dev* dev) {
    unsigned int max_bytes = (unsigned int)dev->bulk_in_mps * USB_MAX_TD;
    unsigned int max_sectors = max_bytes >> 9;
    if (max_sectors == 0u) {
        return 1u;
    }
    if (max_sectors > 127u) {
        max_sectors = 127u;
    }
    return max_sectors;
}

int bios_hdd_read_sectors(unsigned int lba, unsigned int count,
                          unsigned int dest) {
    unsigned int i;

    if (!bios_hdd_present || count == 0u ||
        lba + count > bios_hdd_total_sectors) {
        serial_write_string("13h:r range fail total=");
        serial_write_hex32(bios_hdd_total_sectors);
        serial_write_string("\r\n");
        return -1;
    }
    for (i = 0; i < count;) {
        int rc;
        if (bios_hdd_kind == BIOS_HDD_KIND_IDE) {
            unsigned int chunk = count - i;
            if (bios_hdd_dma_enabled != 0u &&
                bios_hdd_lba48_dma_enabled != 0u) {
                if (chunk > 2048u) {
                    chunk = 2048u;
                }
                if (ide_read_dma_lba48(bios_hdd_io, bios_hdd_ctrl,
                                       bios_hdd_bmio, bios_hdd_drive, lba + i,
                                       chunk,
                                       (unsigned char*)(dest + i * 512u)) == 0) {
                    i += chunk;
                    continue;
                }
                bios_hdd_lba48_dma_enabled = 0u;
                if (bios_hdd_dma_ext_fallback_logged == 0u) {
                    bios_hdd_dma_ext_fallback_logged = 1u;
                    serial_write_string("IDE DMA EXT failed; fallback DMA28\r\n");
                }
                continue;
            }
            if (chunk > 256u) {
                chunk = 256u;
            }
            if (bios_hdd_dma_enabled != 0u &&
                ide_read_dma_lba28(bios_hdd_io, bios_hdd_ctrl, bios_hdd_bmio,
                                   bios_hdd_drive, lba + i, chunk,
                                   (unsigned char*)(dest + i * 512u)) == 0) {
                i += chunk;
                continue;
            }
            if (bios_hdd_dma_enabled != 0u) {
                bios_hdd_dma_enabled = 0u;
                if (bios_hdd_dma_fallback_logged == 0u) {
                    bios_hdd_dma_fallback_logged = 1u;
                    serial_write_string("IDE DMA failed; fallback PIO\r\n");
                }
            }
            rc = ide_read_lba28(bios_hdd_io, bios_hdd_ctrl, bios_hdd_drive,
                                lba + i, chunk,
                                (unsigned char*)(dest + i * 512u));
            if (rc == 0) {
                i += chunk;
                continue;
            }
        } else if (bios_hdd_kind == BIOS_HDD_KIND_USB) {
            unsigned int chunk = count - i;
            unsigned int max_chunk = usb_msd_max_read_sectors(&bios_hdd_usb_dev);
            if (chunk > max_chunk) {
                chunk = max_chunk;
            }
            rc = usb_msd_read_lba_count(&bios_hdd_usb_dev, lba + i, chunk,
                                        (unsigned char*)(dest + i * 512u));
            if (rc == 0) {
                i += chunk;
                continue;
            }
        } else {
            rc = -1;
        }
        if (rc != 0) {
            serial_write_string("13h:r fail lba=");
            serial_write_hex32(lba + i);
            if (bios_hdd_kind == BIOS_HDD_KIND_IDE) {
                serial_write_string(" st=");
                serial_write_hex8(inb((unsigned short)(bios_hdd_io + 7u)));
                serial_write_string(" err=");
                serial_write_hex8(inb((unsigned short)(bios_hdd_io + 1u)));
            }
            serial_write_string("\r\n");
            return -1;
        }
        ++i;
    }
    return 0;
}

int bios_hdd_load_mbr_boot_sector(unsigned int dest) {
    if (!bios_hdd_present || !bios_hdd_has_mbr) {
        return -1;
    }
    if (bios_hdd_read_sectors(0u, 1u, dest) != 0) {
        return -1;
    }
    serial_write_string("Boot HDD MBR kind=");
    serial_write_hex8(bios_hdd_kind);
    serial_write_string("\r\n");
    return 0;
}

static void ide_dump_partition_summary(const unsigned char* sector) {
    unsigned int i;

    serial_write_string("IDE MBR sig=");
    serial_write_hex8(sector[0x01feu]);
    serial_write_hex8(sector[0x01ffu]);
    serial_write_string("\r\n");
    for (i = 0; i < 4u; ++i) {
        const unsigned char* entry = sector + 0x01beu + i * 16u;
        serial_write_string("IDE part");
        serial_write_u32(i);
        serial_write_string(" boot=");
        serial_write_hex8(entry[0]);
        serial_write_string(" type=");
        serial_write_hex8(entry[4]);
        serial_write_string(" start=");
        serial_write_hex32(le32(entry + 8u));
        serial_write_string(" size=");
        serial_write_hex32(le32(entry + 12u));
        serial_write_string("\r\n");
    }
}

static unsigned char storage_sector_has_mbr(const unsigned char* sector) {
    unsigned int i;
    unsigned char has_active = 0u;

    if (sector[0x01feu] != 0x55u || sector[0x01ffu] != 0xaau) {
        return 0u;
    }
    for (i = 0; i < 4u; ++i) {
        const unsigned char* entry = sector + 0x01beu + i * 16u;
        unsigned char boot = entry[0];
        unsigned char type = entry[4];
        unsigned int size = le32(entry + 12u);
        if (boot != 0x00u && boot != 0x80u) {
            return 0u;
        }
        if (boot == 0x80u && type != 0u && size != 0u) {
            has_active = 1u;
        }
    }
    return has_active;
}

static void ide_scan_channel(const char* name, unsigned char channel,
                             unsigned short io, unsigned short ctrl,
                             unsigned short bmio) {
    unsigned char drive;
    unsigned short* id = (unsigned short*)STORAGE_ID_LINEAR;
    unsigned char* sector = (unsigned char*)STORAGE_SECTOR_LINEAR;

    for (drive = 0; drive < 2u; ++drive) {
        unsigned int sectors;
        serial_write_string("IDE ");
        serial_write_string(name);
        serial_write_char(drive == 0u ? 'M' : 'S');
        serial_write_string(" st=");
        serial_write_hex8(inb((unsigned short)(io + 7u)));
        serial_write_string(" alt=");
        serial_write_hex8(inb(ctrl));
        serial_write_string(" identify...");
        if (ide_identify(io, ctrl, drive, id) != 0) {
            serial_write_string(" none\r\n");
            continue;
        }
        sectors = ((unsigned int)id[61] << 16) | id[60];
        serial_write_string(" ok lba28=");
        serial_write_hex32(sectors);
        serial_write_string("\r\n");
        if (sectors != 0u) {
            unsigned char dma_enabled = 0u;
            unsigned char udma = ide_best_udma_mode(id);
            unsigned char mwdma = ide_best_mwdma_mode(id);
            unsigned char lba48 = ide_supports_lba48(id);
            if (bmio != 0u && udma != 0xffu &&
                ide_set_transfer_mode(
                    io, ctrl, drive,
                    (unsigned char)(IDE_XFER_UDMA0 | udma)) == 0) {
                ide_configure_piix4_udma(channel, drive, udma);
                dma_enabled = 1u;
                serial_write_string("  UDMA");
                serial_write_hex8(udma);
                serial_write_string(" enabled bm=");
                serial_write_hex16(bmio);
                serial_write_string(" ctl=");
                serial_write_hex8(storage_ide_udmactl);
                serial_write_string(" tim=");
                serial_write_hex16(storage_ide_udmatim);
                serial_write_string("\r\n");
            } else if (bmio != 0u && mwdma != 0xffu &&
                ide_set_transfer_mode(
                    io, ctrl, drive,
                    (unsigned char)(IDE_XFER_MWDMA0 | mwdma)) == 0) {
                dma_enabled = 1u;
                serial_write_string("  MWDMA");
                serial_write_hex8(mwdma);
                serial_write_string(" enabled bm=");
                serial_write_hex16(bmio);
                serial_write_string("\r\n");
            }
            bios_hdd_register(io, ctrl, bmio, drive, sectors, dma_enabled,
                              (unsigned char)(dma_enabled != 0u && lba48 != 0u));
        }
        if (ide_read_lba28(io, ctrl, drive, 0u, 1u, sector) == 0) {
            serial_dump_bytes("IDE LBA0", sector, 16u);
            if (bios_hdd_kind == BIOS_HDD_KIND_IDE && bios_hdd_io == io &&
                bios_hdd_drive == drive) {
                bios_hdd_has_mbr = storage_sector_has_mbr(sector);
                bios_hdd_save_candidate(&bios_hdd_ide_candidate);
            }
            if (drive == 0u && name[0] == 'p') {
                ide_dump_partition_summary(sector);
            }
        } else {
            serial_write_string("IDE LBA0 read failed\r\n");
        }
    }
}

static void ide_enable_piix4_legacy(void) {
    unsigned short cmd;
    unsigned short bmiba;
    unsigned short primary_timing;
    unsigned short secondary_timing;

    cmd = pci_read16(storage_ide_bdf.bus, storage_ide_bdf.dev,
                     storage_ide_bdf.fn, 0x04u);
    pci_write16(storage_ide_bdf.bus, storage_ide_bdf.dev, storage_ide_bdf.fn,
                0x04u, (unsigned short)(cmd | 0x0005u));

    primary_timing = pci_read16(storage_ide_bdf.bus, storage_ide_bdf.dev,
                                storage_ide_bdf.fn, 0x40u);
    secondary_timing = pci_read16(storage_ide_bdf.bus, storage_ide_bdf.dev,
                                  storage_ide_bdf.fn, 0x42u);
    pci_write16(storage_ide_bdf.bus, storage_ide_bdf.dev, storage_ide_bdf.fn,
                0x40u, (unsigned short)(primary_timing | 0x8000u));
    pci_write16(storage_ide_bdf.bus, storage_ide_bdf.dev, storage_ide_bdf.fn,
                0x42u, (unsigned short)(secondary_timing | 0x8000u));

    bmiba = (unsigned short)(pci_read16(storage_ide_bdf.bus, storage_ide_bdf.dev,
                                        storage_ide_bdf.fn, 0x20u) &
                             0xfff0u);
    storage_ide_bmiba = bmiba;
    storage_ide_udmactl = 0u;
    storage_ide_udmatim = 0u;
    pci_write8(storage_ide_bdf.bus, storage_ide_bdf.dev, storage_ide_bdf.fn,
               0x48u, storage_ide_udmactl);
    pci_write16(storage_ide_bdf.bus, storage_ide_bdf.dev, storage_ide_bdf.fn,
                0x4au, storage_ide_udmatim);
    serial_write_string("IDE cfg cmd=");
    serial_write_hex16(pci_read16(storage_ide_bdf.bus, storage_ide_bdf.dev,
                                  storage_ide_bdf.fn, 0x04u));
    serial_write_string(" bmiba=");
    serial_write_hex16(bmiba);
    serial_write_string(" pri=");
    serial_write_hex16(pci_read16(storage_ide_bdf.bus, storage_ide_bdf.dev,
                                  storage_ide_bdf.fn, 0x40u));
    serial_write_string(" sec=");
    serial_write_hex16(pci_read16(storage_ide_bdf.bus, storage_ide_bdf.dev,
                                  storage_ide_bdf.fn, 0x42u));
    serial_write_string(" udma=");
    serial_write_hex8(pci_read8(storage_ide_bdf.bus, storage_ide_bdf.dev,
                                storage_ide_bdf.fn, 0x48u));
    serial_write_char('/');
    serial_write_hex16(storage_ide_udmatim);
    serial_write_string("\r\n");
}

static void ide_scan(void) {
    if (!storage_ide_found) {
        serial_write_string("IDE: controller not found\r\n");
        return;
    }
    ide_enable_piix4_legacy();
    serial_write_string("IDE scan ");
    pci_print_bdf(storage_ide_bdf.bus, storage_ide_bdf.dev, storage_ide_bdf.fn);
    serial_write_string("\r\n");
    ide_scan_channel("pri", 0u, 0x01f0u, 0x03f6u, storage_ide_bmiba);
    ide_scan_channel("sec", 1u, 0x0170u, 0x0376u,
                     storage_ide_bmiba != 0u
                         ? (unsigned short)(storage_ide_bmiba + 8u)
                         : 0u);
}

struct uhci_td {
    volatile unsigned int link;
    volatile unsigned int status;
    volatile unsigned int token;
    volatile unsigned int buffer;
    volatile unsigned int sw[4];
};

struct uhci_qh {
    volatile unsigned int head;
    volatile unsigned int element;
};

#define UHCI_USBCMD 0x00u
#define UHCI_USBSTS 0x02u
#define UHCI_USBINTR 0x04u
#define UHCI_FRNUM 0x06u
#define UHCI_FLBASEADD 0x08u
#define UHCI_SOFMOD 0x0cu
#define UHCI_PORTSC1 0x10u

#define UHCI_CMD_RS 0x0001u
#define UHCI_CMD_HCRESET 0x0002u
#define UHCI_CMD_CF 0x0040u
#define UHCI_CMD_MAXP 0x0080u

#define UHCI_PORT_CCS 0x0001u
#define UHCI_PORT_CSC 0x0002u
#define UHCI_PORT_PE 0x0004u
#define UHCI_PORT_PEC 0x0008u
#define UHCI_PORT_LSDA 0x0100u
#define UHCI_PORT_PR 0x0200u
#define UHCI_PORT_CHANGE (UHCI_PORT_CSC | UHCI_PORT_PEC)

#define UHCI_PTR_TERM 0x00000001u
#define UHCI_PTR_QH 0x00000002u
#define UHCI_PTR_DEPTH 0x00000004u

#define UHCI_TD_ACTIVE 0x00800000u
#define UHCI_TD_STALLED 0x00400000u
#define UHCI_TD_DBE 0x00200000u
#define UHCI_TD_BABBLE 0x00100000u
#define UHCI_TD_NAK 0x00080000u
#define UHCI_TD_CRC_TIMEOUT 0x00040000u
#define UHCI_TD_BITSTUFF 0x00020000u
#define UHCI_TD_ERR3 0x18000000u
#define UHCI_TD_LOW_SPEED 0x04000000u

#define USB_PID_OUT 0xe1u
#define USB_PID_IN 0x69u
#define USB_PID_SETUP 0x2du

static struct uhci_td* uhci_td_base(void) {
    return (struct uhci_td*)USB_TD_LINEAR;
}

static struct uhci_qh* uhci_qh(void) { return (struct uhci_qh*)USB_QH_LINEAR; }

static unsigned int uhci_td_phys(unsigned int index) {
    return USB_TD_LINEAR + index * sizeof(struct uhci_td);
}

static unsigned int uhci_token(unsigned char pid, unsigned char addr,
                               unsigned char ep, unsigned char toggle,
                               unsigned int len) {
    unsigned int max_len = (len == 0u) ? 0x7ffu : (len - 1u);
    return (unsigned int)pid | ((unsigned int)addr << 8) |
           ((unsigned int)(ep & 0x0fu) << 15) |
           ((unsigned int)(toggle & 1u) << 19) | (max_len << 21);
}

static void uhci_prepare_schedule(unsigned short io) {
    volatile unsigned int* frame = (volatile unsigned int*)USB_FRAME_LIST_LINEAR;
    struct uhci_qh* qh = uhci_qh();
    unsigned int i;

    for (i = 0; i < 1024u; ++i) {
        frame[i] = USB_QH_LINEAR | UHCI_PTR_QH;
    }
    qh->head = UHCI_PTR_TERM;
    qh->element = UHCI_PTR_TERM;
    storage_memset((void*)USB_TD_LINEAR, 0u,
                   USB_MAX_TD * sizeof(struct uhci_td));

    outw((unsigned short)(io + UHCI_USBCMD), 0u);
    outw((unsigned short)(io + UHCI_USBSTS), 0x003fu);
    outw((unsigned short)(io + UHCI_USBINTR), 0u);
    outw((unsigned short)(io + UHCI_FRNUM), 0u);
    outl((unsigned short)(io + UHCI_FLBASEADD), USB_FRAME_LIST_LINEAR);
    outb((unsigned short)(io + UHCI_SOFMOD), 0x40u);
}

static void uhci_td_setup(unsigned int index, unsigned int link,
                          unsigned int status, unsigned int token,
                          unsigned int buffer) {
    struct uhci_td* td = uhci_td_base() + index;
    td->link = link;
    td->status = status;
    td->token = token;
    td->buffer = buffer;
    td->sw[0] = td->sw[1] = td->sw[2] = td->sw[3] = 0u;
}

static int uhci_run_chain(unsigned short io, unsigned int td_count) {
    struct uhci_td* td = uhci_td_base();
    struct uhci_qh* qh = uhci_qh();
    unsigned int timeout = 4000000u;
    unsigned int i;

    if (td_count == 0u || td_count > USB_MAX_TD) {
        return -1;
    }

    qh->head = UHCI_PTR_TERM;
    qh->element = uhci_td_phys(0);
    cache_writeback_invalidate();

    outw((unsigned short)(io + UHCI_USBSTS), 0x003fu);
    outw((unsigned short)(io + UHCI_USBCMD),
         UHCI_CMD_CF | UHCI_CMD_MAXP | UHCI_CMD_RS);

    while (timeout-- != 0u) {
        if ((td[td_count - 1u].status & UHCI_TD_ACTIVE) == 0u) {
            break;
        }
        if ((timeout & 0x3ffu) == 0u) {
            (void)inw((unsigned short)(io + UHCI_USBSTS));
        }
    }

    outw((unsigned short)(io + UHCI_USBCMD), UHCI_CMD_CF | UHCI_CMD_MAXP);
    cache_writeback_invalidate();

    for (i = 0; i < td_count; ++i) {
        unsigned int st = td[i].status;
        if ((st & (UHCI_TD_ACTIVE | UHCI_TD_STALLED | UHCI_TD_DBE |
                   UHCI_TD_BABBLE | UHCI_TD_CRC_TIMEOUT |
                   UHCI_TD_BITSTUFF)) != 0u) {
            serial_write_string("UHCI td");
            serial_write_u32(i);
            serial_write_string(" st=");
            serial_write_hex32(st);
            serial_write_string(" tok=");
            serial_write_hex32(td[i].token);
            serial_write_string(" us=");
            serial_write_hex16(inw((unsigned short)(io + UHCI_USBSTS)));
            serial_write_string("\r\n");
            return -1;
        }
    }
    return 0;
}

static int uhci_control(struct usb_dev* dev, unsigned char req_type,
                        unsigned char req, unsigned short value,
                        unsigned short index, void* data, unsigned int len,
                        unsigned char dir_in) {
    unsigned char* setup = (unsigned char*)USB_BUF_LINEAR;
    unsigned int td_count = 0;
    unsigned int offset = 0;
    unsigned char toggle = 1u;
    unsigned int status = UHCI_TD_ACTIVE | UHCI_TD_ERR3 |
                          (dev->low_speed ? UHCI_TD_LOW_SPEED : 0u);

    setup[0] = req_type;
    setup[1] = req;
    put16le(setup + 2, value);
    put16le(setup + 4, index);
    put16le(setup + 6, (unsigned short)len);

    uhci_td_setup(td_count, uhci_td_phys(1u) | UHCI_PTR_DEPTH, status,
                  uhci_token(USB_PID_SETUP, dev->addr, 0u, 0u, 8u),
                  USB_BUF_LINEAR);
    ++td_count;

    while (offset < len) {
        unsigned int chunk = len - offset;
        unsigned char pid = dir_in ? USB_PID_IN : USB_PID_OUT;
        if (chunk > dev->ep0_mps) {
            chunk = dev->ep0_mps;
        }
        if (td_count + 1u >= USB_MAX_TD) {
            return -1;
        }
        uhci_td_setup(td_count, uhci_td_phys(td_count + 1u) | UHCI_PTR_DEPTH,
                      status, uhci_token(pid, dev->addr, 0u, toggle, chunk),
                      (unsigned int)data + offset);
        toggle ^= 1u;
        offset += chunk;
        ++td_count;
    }

    uhci_td_setup(td_count, UHCI_PTR_TERM, status,
                  uhci_token(dir_in ? USB_PID_OUT : USB_PID_IN, dev->addr, 0u,
                             1u, 0u),
                  0u);
    ++td_count;
    return uhci_run_chain(dev->io, td_count);
}

static int uhci_bulk(struct usb_dev* dev, unsigned char ep,
                     unsigned short mps, unsigned char dir_in, void* data,
                     unsigned int len, unsigned char* toggle_ptr) {
    unsigned int td_count = 0;
    unsigned int offset = 0;
    unsigned int status = UHCI_TD_ACTIVE | UHCI_TD_ERR3 |
                          (dev->low_speed ? UHCI_TD_LOW_SPEED : 0u);
    unsigned char pid = dir_in ? USB_PID_IN : USB_PID_OUT;

    while (offset < len) {
        unsigned int chunk = len - offset;
        unsigned int link;
        if (chunk > mps) {
            chunk = mps;
        }
        if (td_count >= USB_MAX_TD) {
            return -1;
        }
        link = (offset + chunk < len)
                   ? (uhci_td_phys(td_count + 1u) | UHCI_PTR_DEPTH)
                   : UHCI_PTR_TERM;
        uhci_td_setup(td_count, link, status,
                      uhci_token(pid, dev->addr, ep, *toggle_ptr, chunk),
                      (unsigned int)data + offset);
        *toggle_ptr ^= 1u;
        offset += chunk;
        ++td_count;
    }
    return uhci_run_chain(dev->io, td_count);
}

static int uhci_reset_port(unsigned short io, unsigned char port_index,
                           unsigned char* low_speed) {
    unsigned short port = (unsigned short)(io + UHCI_PORTSC1 + port_index * 2u);
    unsigned short st = inw(port);

    serial_write_string("UHCI port");
    serial_write_u32(port_index);
    serial_write_string("=");
    serial_write_hex16(st);
    serial_write_string("\r\n");
    if ((st & UHCI_PORT_CCS) == 0u) {
        return -1;
    }

    outw(port, (unsigned short)(st | UHCI_PORT_PR | UHCI_PORT_CHANGE));
    delay_approx_ms(50u);
    st = inw(port);
    outw(port, (unsigned short)((st & ~UHCI_PORT_PR) | UHCI_PORT_CHANGE));
    delay_approx_ms(10u);
    st = inw(port);
    outw(port, (unsigned short)(st | UHCI_PORT_PE | UHCI_PORT_CHANGE));
    delay_approx_ms(20u);
    st = inw(port);

    serial_write_string("UHCI port");
    serial_write_u32(port_index);
    serial_write_string("*=");
    serial_write_hex16(st);
    serial_write_string("\r\n");
    if ((st & UHCI_PORT_PE) == 0u) {
        return -1;
    }
    *low_speed = (st & UHCI_PORT_LSDA) != 0u ? 1u : 0u;
    return 0;
}

static int uhci_controller_reset(unsigned short io) {
    unsigned int timeout = 1000000u;
    outw((unsigned short)(io + UHCI_USBCMD), UHCI_CMD_HCRESET);
    while ((inw((unsigned short)(io + UHCI_USBCMD)) & UHCI_CMD_HCRESET) != 0u) {
        if (--timeout == 0u) {
            return -1;
        }
    }
    delay_approx_ms(10u);
    uhci_prepare_schedule(io);
    return 0;
}

static int usb_get_descriptor(struct usb_dev* dev, unsigned char type,
                              unsigned char index, void* data,
                              unsigned int len) {
    return uhci_control(dev, 0x80u, 0x06u,
                        (unsigned short)(((unsigned short)type << 8) | index),
                        0u, data, len, 1u);
}

static int usb_set_address(struct usb_dev* dev, unsigned char addr) {
    if (uhci_control(dev, 0x00u, 0x05u, addr, 0u, 0, 0u, 0u) != 0) {
        return -1;
    }
    delay_approx_ms(10u);
    dev->addr = addr;
    return 0;
}

static int usb_set_configuration(struct usb_dev* dev, unsigned char cfg) {
    return uhci_control(dev, 0x00u, 0x09u, cfg, 0u, 0, 0u, 0u);
}

static int usb_parse_config(struct usb_dev* dev, unsigned char* cfg,
                            unsigned int total) {
    unsigned int off = 0;
    unsigned char in_mass = 0;
    unsigned char cfg_value = cfg[5];

    dev->bulk_in = 0;
    dev->bulk_out = 0;
    dev->bulk_in_mps = 0;
    dev->bulk_out_mps = 0;

    while (off + 2u <= total) {
        unsigned char len = cfg[off];
        unsigned char type = cfg[off + 1u];
        if (len < 2u || off + len > total) {
            break;
        }
        if (type == 0x04u && len >= 9u) {
            in_mass = (cfg[off + 5u] == 0x08u && cfg[off + 7u] == 0x50u)
                          ? 1u
                          : 0u;
            if (in_mass) {
                dev->interface_number = cfg[off + 2u];
                serial_write_string("USB MSC if=");
                serial_write_hex8(dev->interface_number);
                serial_write_string(" sub=");
                serial_write_hex8(cfg[off + 6u]);
                serial_write_string("\r\n");
            }
        } else if (type == 0x05u && len >= 7u && in_mass) {
            unsigned char ep = cfg[off + 2u];
            unsigned char attr = cfg[off + 3u];
            unsigned short mps = le16(cfg + off + 4u);
            if ((attr & 0x03u) == 0x02u) {
                if ((ep & 0x80u) != 0u) {
                    dev->bulk_in = (unsigned char)(ep & 0x0fu);
                    dev->bulk_in_mps = mps;
                } else {
                    dev->bulk_out = (unsigned char)(ep & 0x0fu);
                    dev->bulk_out_mps = mps;
                }
            }
        }
        off += len;
    }

    if (dev->bulk_in == 0u || dev->bulk_out == 0u || dev->bulk_in_mps == 0u ||
        dev->bulk_out_mps == 0u) {
        return -1;
    }

    serial_write_string("USB bulk in=");
    serial_write_hex8(dev->bulk_in);
    serial_write_string(" out=");
    serial_write_hex8(dev->bulk_out);
    serial_write_string(" mps=");
    serial_write_hex16(dev->bulk_in_mps);
    serial_write_char('/');
    serial_write_hex16(dev->bulk_out_mps);
    serial_write_string(" cfg=");
    serial_write_hex8(cfg_value);
    serial_write_string("\r\n");

    if (usb_set_configuration(dev, cfg_value) != 0) {
        return -1;
    }
    dev->bulk_in_toggle = 0;
    dev->bulk_out_toggle = 0;
    delay_approx_ms(50u);
    return 0;
}

static int usb_enumerate_device(struct usb_dev* dev) {
    unsigned char* desc = (unsigned char*)USB_CFG_BUF_LINEAR;
    unsigned int total;

    dev->addr = 0;
    dev->ep0_mps = 8u;
    if (usb_get_descriptor(dev, 0x01u, 0u, desc, 8u) != 0) {
        return -1;
    }
    if (desc[0] < 8u || desc[1] != 0x01u) {
        return -1;
    }
    dev->ep0_mps = desc[7];
    if (dev->ep0_mps != 8u && dev->ep0_mps != 16u && dev->ep0_mps != 32u &&
        dev->ep0_mps != 64u) {
        dev->ep0_mps = 8u;
    }
    serial_write_string("USB ep0=");
    serial_write_u32(dev->ep0_mps);
    serial_write_string("\r\n");

    if (usb_set_address(dev, 1u) != 0) {
        return -1;
    }
    if (usb_get_descriptor(dev, 0x01u, 0u, desc, 18u) != 0) {
        return -1;
    }
    serial_write_string("USB dev ");
    serial_write_hex16(le16(desc + 8u));
    serial_write_char(':');
    serial_write_hex16(le16(desc + 10u));
    serial_write_string("\r\n");

    if (usb_get_descriptor(dev, 0x02u, 0u, desc, 9u) != 0) {
        return -1;
    }
    total = le16(desc + 2u);
    if (total > 512u) {
        total = 512u;
    }
    if (usb_get_descriptor(dev, 0x02u, 0u, desc, total) != 0) {
        return -1;
    }
    return usb_parse_config(dev, desc, total);
}

static int usb_msd_command(struct usb_dev* dev, const unsigned char* cdb,
                           unsigned int cdb_len, unsigned char dir_in,
                           void* data, unsigned int data_len) {
    unsigned char* cbw = (unsigned char*)USB_CBW_BUF_LINEAR;
    unsigned char* csw = (unsigned char*)USB_CSW_BUF_LINEAR;
    unsigned int tag = 0x440b0001u;
    unsigned int i;

    storage_memset(cbw, 0u, 31u);
    put32le(cbw + 0u, 0x43425355u);
    put32le(cbw + 4u, tag);
    put32le(cbw + 8u, data_len);
    cbw[12] = dir_in ? 0x80u : 0x00u;
    cbw[13] = 0u;
    cbw[14] = (unsigned char)cdb_len;
    for (i = 0; i < cdb_len && i < 16u; ++i) {
        cbw[15u + i] = cdb[i];
    }

    if (uhci_bulk(dev, dev->bulk_out, dev->bulk_out_mps, 0u, cbw, 31u,
                  &dev->bulk_out_toggle) != 0) {
        return -1;
    }
    if (data_len != 0u) {
        if (dir_in) {
            if (uhci_bulk(dev, dev->bulk_in, dev->bulk_in_mps, 1u, data,
                          data_len, &dev->bulk_in_toggle) != 0) {
                return -1;
            }
        } else {
            if (uhci_bulk(dev, dev->bulk_out, dev->bulk_out_mps, 0u, data,
                          data_len, &dev->bulk_out_toggle) != 0) {
                return -1;
            }
        }
    }
    storage_memset(csw, 0u, 16u);
    if (uhci_bulk(dev, dev->bulk_in, dev->bulk_in_mps, 1u, csw, 13u,
                  &dev->bulk_in_toggle) != 0) {
        return -1;
    }
    if (le32(csw) != 0x53425355u || le32(csw + 4u) != tag || csw[12] != 0u) {
        if (usb_msd_quiet_status) {
            return -1;
        }
        serial_write_string("USB CSW bad sig=");
        serial_write_hex32(le32(csw));
        serial_write_string(" tag=");
        serial_write_hex32(le32(csw + 4u));
        serial_write_string(" st=");
        serial_write_hex8(csw[12]);
        serial_write_string("\r\n");
        return -1;
    }
    return 0;
}

static void usb_msd_request_sense(struct usb_dev* dev) {
    unsigned char cdb[6];
    unsigned char* sense = (unsigned char*)USB_CFG_BUF_LINEAR;

    storage_memset(cdb, 0u, sizeof(cdb));
    storage_memset(sense, 0u, 18u);
    cdb[0] = 0x03u;
    cdb[4] = 18u;
    if (usb_msd_command(dev, cdb, 6u, 1u, sense, 18u) == 0) {
        serial_dump_bytes("USB sense", sense, 18u);
    }
}

static int usb_msd_read_capacity(struct usb_dev* dev, unsigned int* sectors) {
    unsigned char cdb[10];
    unsigned char* cap = (unsigned char*)USB_CFG_BUF_LINEAR;
    unsigned int last_lba;
    unsigned int block_len;

    storage_memset(cdb, 0u, sizeof(cdb));
    storage_memset(cap, 0u, 8u);
    cdb[0] = 0x25u;
    if (usb_msd_command(dev, cdb, 10u, 1u, cap, 8u) == 0) {
        last_lba = ((unsigned int)cap[0] << 24) |
                   ((unsigned int)cap[1] << 16) |
                   ((unsigned int)cap[2] << 8) | cap[3];
        block_len = ((unsigned int)cap[4] << 24) |
                    ((unsigned int)cap[5] << 16) |
                    ((unsigned int)cap[6] << 8) | cap[7];
        serial_write_string("USB capacity last=");
        serial_write_hex32(last_lba);
        serial_write_string(" blksz=");
        serial_write_hex32(block_len);
        serial_write_string("\r\n");
        if (block_len != 512u || last_lba == 0xffffffffu) {
            return -1;
        }
        *sectors = last_lba + 1u;
        return 0;
    } else {
        usb_msd_request_sense(dev);
    }
    return -1;
}

static void usb_msd_test_unit_ready(struct usb_dev* dev) {
    unsigned char cdb[6];

    storage_memset(cdb, 0u, sizeof(cdb));
    cdb[0] = 0x00u;
    usb_msd_quiet_status = 1u;
    if (usb_msd_command(dev, cdb, 6u, 0u, 0, 0u) != 0) {
        usb_msd_quiet_status = 0u;
        usb_msd_request_sense(dev);
    } else {
        usb_msd_quiet_status = 0u;
    }
}

static int usb_msd_read_lba_count(struct usb_dev* dev, unsigned int lba,
                                  unsigned int count, unsigned char* sector) {
    unsigned char cdb[10];
    unsigned int attempt;

    if (count == 0u || count > usb_msd_max_read_sectors(dev)) {
        return -1;
    }
    storage_memset(cdb, 0u, sizeof(cdb));
    cdb[0] = 0x28u;
    put32be(cdb + 2u, lba);
    put16be(cdb + 7u, (unsigned short)count);
    for (attempt = 0; attempt < 3u; ++attempt) {
        if (usb_msd_command(dev, cdb, 10u, 1u, sector, count * 512u) == 0) {
            return 0;
        }
        usb_msd_request_sense(dev);
    }
    return -1;
}

static void usb_scan(void) {
    unsigned int bar4;
    unsigned short io;
    unsigned char port;

    if (!storage_uhci_found) {
        serial_write_string("USB: UHCI not found\r\n");
        return;
    }
    pci_write16(storage_uhci_bdf.bus, storage_uhci_bdf.dev, storage_uhci_bdf.fn,
                0x04u,
                (unsigned short)(pci_read16(storage_uhci_bdf.bus,
                                            storage_uhci_bdf.dev,
                                            storage_uhci_bdf.fn, 0x04u) |
                                 0x0005u));
    bar4 = pci_read32(storage_uhci_bdf.bus, storage_uhci_bdf.dev,
                      storage_uhci_bdf.fn, 0x20u);
    io = (unsigned short)(bar4 & 0xffe0u);
    if (io == 0u) {
        io = (unsigned short)align_up_u32(pci_next_io, 0x20u);
        pci_next_io = io + 0x20u;
        pci_write32(storage_uhci_bdf.bus, storage_uhci_bdf.dev,
                    storage_uhci_bdf.fn, 0x20u, (unsigned int)io | 1u);
    }

    serial_write_string("USB UHCI ");
    pci_print_bdf(storage_uhci_bdf.bus, storage_uhci_bdf.dev,
                  storage_uhci_bdf.fn);
    serial_write_string(" io=");
    serial_write_hex16(io);
    serial_write_string("\r\n");

    if (uhci_controller_reset(io) != 0) {
        serial_write_string("UHCI reset failed\r\n");
        return;
    }

    for (port = 0; port < 2u; ++port) {
        struct usb_dev dev;
        unsigned char low_speed = 0;
        unsigned char* sector = (unsigned char*)USB_SECTOR_LINEAR;
        unsigned int sectors = 0;
        if (uhci_reset_port(io, port, &low_speed) != 0) {
            continue;
        }
        storage_memset(&dev, 0u, sizeof(dev));
        dev.io = io;
        dev.low_speed = low_speed;
        if (usb_enumerate_device(&dev) != 0) {
            serial_write_string("USB enum failed\r\n");
            continue;
        }
        usb_msd_test_unit_ready(&dev);
        if (usb_msd_read_capacity(&dev, &sectors) != 0) {
            serial_write_string("USB capacity failed\r\n");
            continue;
        }
        if (usb_msd_read_lba_count(&dev, 0u, 1u, sector) == 0) {
            unsigned char has_mbr = storage_sector_has_mbr(sector);
            serial_dump_bytes("USB LBA0", sector, 16u);
            bios_hdd_register_usb(&dev, sectors, has_mbr);
        } else {
            serial_write_string("USB LBA0 read failed\r\n");
        }
    }
}

void storage_scan(unsigned int total_bytes) {
    if (total_bytes < 0x00800000u) {
        serial_write_string("Storage scan skipped: low DRAM\r\n");
        return;
    }
    bios_hdd_present = 0u;
    bios_hdd_kind = BIOS_HDD_KIND_NONE;
    storage_memset(&bios_hdd_ide_candidate, 0u, sizeof(bios_hdd_ide_candidate));
    storage_memset(&bios_hdd_usb_candidate, 0u, sizeof(bios_hdd_usb_candidate));
    pci_enumerate_and_assign(total_bytes);
    ide_scan();
    usb_scan();
    if (bios_hdd_ide_candidate.present != 0u) {
        bios_hdd_activate_candidate(&bios_hdd_ide_candidate);
    } else if (bios_hdd_usb_candidate.present != 0u) {
        bios_hdd_activate_candidate(&bios_hdd_usb_candidate);
    }
}
