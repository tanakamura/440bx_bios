#include "stage3/pci_snapshot.h"

#include "bios_pci.h"
#include "shared_service/service_table.h"

struct pci_snapshot_build_state {
    struct shared_pci_snapshot* snapshot;
    unsigned char visited_bus[256];
};

static unsigned int pci_class_code(unsigned char bus, unsigned char dev,
                                   unsigned char fn) {
    return pci_read32(bus, dev, fn, 0x08u) >> 8;
}

static void pci_snapshot_fill_device(struct shared_pci_device* out,
                                     unsigned char bus, unsigned char dev,
                                     unsigned char fn,
                                     unsigned short parent_index) {
    unsigned int i;
    unsigned char header_type = pci_read8(bus, dev, fn, 0x0eu);

    *out = (struct shared_pci_device){0};
    out->vendor_device = pci_read32(bus, dev, fn, 0x00u);
    out->command = pci_read16(bus, dev, fn, 0x04u);
    out->status = pci_read16(bus, dev, fn, 0x06u);
    out->class_code = pci_class_code(bus, dev, fn);
    for (i = 0u; i < 6u; ++i) {
        out->bars[i] = pci_read32(bus, dev, fn, (unsigned char)(0x10u + i * 4u));
    }
    out->parent_index = parent_index;
    out->bus = bus;
    out->dev = dev;
    out->fn = fn;
    out->header_type = header_type;
    out->interrupt_line = pci_read8(bus, dev, fn, 0x3cu);
    out->interrupt_pin = pci_read8(bus, dev, fn, 0x3du);
    if ((header_type & 0x7fu) == 0x01u) {
        out->secondary_bus = pci_read8(bus, dev, fn, 0x19u);
        out->subordinate_bus = pci_read8(bus, dev, fn, 0x1au);
    }
}

static void pci_snapshot_scan_bus(struct pci_snapshot_build_state* state,
                                  unsigned char bus,
                                  unsigned short parent_index) {
    unsigned char dev;

    if (state == 0 || state->snapshot == 0 || state->snapshot->count >=
                                                  SHARED_PCI_SNAPSHOT_MAX_DEVICES ||
        state->visited_bus[bus] != 0u) {
        return;
    }
    state->visited_bus[bus] = 1u;

    for (dev = 0u; dev < 32u; ++dev) {
        unsigned char fn_limit = 1u;
        unsigned char fn;

        if ((pci_read32(bus, dev, 0u, 0x00u) & 0xffffu) == 0xffffu) {
            continue;
        }
        if ((pci_read8(bus, dev, 0u, 0x0eu) & 0x80u) != 0u) {
            fn_limit = 8u;
        }
        for (fn = 0u; fn < fn_limit; ++fn) {
            unsigned int vendor_device = pci_read32(bus, dev, fn, 0x00u);
            unsigned short index;
            struct shared_pci_device* device;

            if ((vendor_device & 0xffffu) == 0xffffu) {
                continue;
            }
            if (state->snapshot->count >= SHARED_PCI_SNAPSHOT_MAX_DEVICES) {
                return;
            }
            index = (unsigned short)state->snapshot->count++;
            device = &state->snapshot->devices[index];
            pci_snapshot_fill_device(device, bus, dev, fn, parent_index);
            if ((device->header_type & 0x7fu) == 0x01u &&
                device->secondary_bus != 0u &&
                device->secondary_bus <= device->subordinate_bus) {
                pci_snapshot_scan_bus(state, device->secondary_bus, index);
            }
        }
    }
}

int stage3_pci_snapshot_build(unsigned int total_bytes,
                              struct shared_service_table* shared) {
    shared_heap_alloc_fn alloc;
    struct shared_pci_snapshot* snapshot;
    struct pci_snapshot_build_state state;

    if (shared == 0 || shared->heap_alloc == 0u) {
        return -1;
    }
    alloc = (shared_heap_alloc_fn)shared->heap_alloc;
    snapshot = (struct shared_pci_snapshot*)alloc(total_bytes, sizeof(*snapshot));
    if (snapshot == 0) {
        shared->pci_snapshot_ptr = 0u;
        return -1;
    }
    *snapshot = (struct shared_pci_snapshot){0};
    snapshot->magic = SHARED_PCI_SNAPSHOT_MAGIC;
    snapshot->version = SHARED_PCI_SNAPSHOT_VERSION;
    snapshot->size = sizeof(*snapshot);

    state.snapshot = snapshot;
    {
        unsigned int i;
        for (i = 0u; i < 256u; ++i) {
            state.visited_bus[i] = 0u;
        }
    }
    pci_snapshot_scan_bus(&state, 0u, 0xffffu);
    shared->pci_snapshot_ptr = (unsigned int)snapshot;
    return 0;
}
