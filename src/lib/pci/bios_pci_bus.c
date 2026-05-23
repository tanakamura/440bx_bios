#include "bios_pci_bus.h"

#include "bios_io.h"
#include "bios_pci.h"
#include "bios_serial.h"
#include "post_code.h"
#include "shared_service/service_table.h"

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
static unsigned char pci_bridge_count = 0u;

static unsigned int align_up_u32(unsigned int value, unsigned int align) {
    if (align == 0u) {
        return value;
    }
    return (value + align - 1u) & ~(align - 1u);
}

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

    *info = (struct pci_bar_info){0};
    info->index = bar_index;

    orig = pci_read32(bus, dev, fn, reg);
    if (orig == 0xffffffffu) {
        return 0;
    }
    command = pci_read16(bus, dev, fn, 0x04u);
    pci_write16(bus, dev, fn, 0x04u, (unsigned short)(command & ~0x0003u));
    if ((orig & 0x00000001u) == 0u && (orig & 0x00000006u) == 0x00000004u &&
        (bar_index + 1u) < bar_count) {
        orig_hi = pci_read32(bus, dev, fn, (unsigned char)(reg + 4u));
        pci_write32(bus, dev, fn, (unsigned char)(reg + 4u), 0xffffffffu);
    }
    pci_write32(bus, dev, fn, reg, 0xffffffffu);
    mask = pci_read32(bus, dev, fn, reg);
    pci_write32(bus, dev, fn, reg, orig);
    if ((orig & 0x00000001u) == 0u && (orig & 0x00000006u) == 0x00000004u &&
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

                bridge->req.io = pci_window_size(child.io, PCI_BRIDGE_IO_ALIGN);
                bridge->req.mem = pci_window_size(child.mem, PCI_BRIDGE_MEM_ALIGN);
                bridge->req.pref = pci_window_size(child.pref, PCI_BRIDGE_MEM_ALIGN);
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
                    pref_base = align_up_u32(alloc->pref, PCI_BRIDGE_MEM_ALIGN);
                    alloc->pref = pref_base + bridge->req.pref;
                }
                pci_bridge_set_io_window(bus, dev, fn, io_base, bridge->req.io);
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
                    bridge_control = (unsigned short)(bridge_control | 0x000cu);
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

void pci_bus_enumerate_and_assign(unsigned int total_bytes,
                                  struct shared_boot_context* boot) {
    struct pci_resource_req root_req;
    struct pci_allocator root_alloc;
    unsigned char next_bus = 1u;
    unsigned char i;
    unsigned int mem_base;

    outb(0x80, POST_PCI_PROBE_START);
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
        align_up_u32(mem_base + pci_window_size(root_req.mem, PCI_ROOT_MEM_ALIGN),
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
    if (boot != 0) {
        boot->pci_io_base = PCI_ROOT_IO_BASE;
        boot->pci_io_limit = root_alloc.io;
        boot->pci_mem_base = mem_base;
        boot->pci_mem_limit = root_alloc.mem;
        boot->pci_prefetch_mem_base =
            align_up_u32(mem_base + pci_window_size(root_req.mem, PCI_ROOT_MEM_ALIGN),
                         PCI_ROOT_MEM_ALIGN);
        boot->pci_prefetch_mem_limit = root_alloc.pref;
    }
}
