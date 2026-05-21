#include "bios_memory.h"

unsigned int bios_memory_top_reserved_base(unsigned int total_bytes) {
    return bios_memory_top_reserved_base_value(total_bytes);
}

unsigned int bios_memory_extended_usable_end(unsigned int total_bytes) {
    unsigned int top_reserved = bios_memory_top_reserved_base(total_bytes);
    if (top_reserved > total_bytes) {
        return total_bytes;
    }
    return top_reserved;
}

unsigned int bios_memory_e820_entry_count(unsigned int total_bytes) {
    if (total_bytes <= BIOS_BASE_MEMORY_LIMIT) {
        return 2u;
    }
    if (bios_memory_top_reserved_base(total_bytes) > BIOS_BASE_MEMORY_LIMIT) {
        return 4u;
    }
    return 3u;
}

static void e820_set_entry(struct e820_entry* entry, unsigned int base,
                           unsigned int length, unsigned int type) {
    entry->base_low = base;
    entry->base_high = 0u;
    entry->length_low = length;
    entry->length_high = 0u;
    entry->type = type;
}

int bios_memory_e820_get_entry(unsigned int total_bytes, unsigned int index,
                               struct e820_entry* entry) {
    unsigned int usable_end = bios_memory_extended_usable_end(total_bytes);
    unsigned int top_reserved = bios_memory_top_reserved_base(total_bytes);

    switch (index) {
        case 0:
            e820_set_entry(entry, 0x00000000u, 0x0009fc00u,
                           BIOS_E820_TYPE_USABLE);
            return 0;
        case 1:
            e820_set_entry(entry, 0x0009fc00u, 0x00060400u,
                           BIOS_E820_TYPE_RESERVED);
            return 0;
        case 2:
            if (total_bytes <= BIOS_BASE_MEMORY_LIMIT) {
                return -1;
            }
            if (usable_end <= BIOS_BASE_MEMORY_LIMIT) {
                e820_set_entry(entry, BIOS_BASE_MEMORY_LIMIT,
                               total_bytes - BIOS_BASE_MEMORY_LIMIT,
                               BIOS_E820_TYPE_RESERVED);
            } else {
                e820_set_entry(entry, BIOS_BASE_MEMORY_LIMIT,
                               usable_end - BIOS_BASE_MEMORY_LIMIT,
                               BIOS_E820_TYPE_USABLE);
            }
            return 0;
        case 3:
            if (top_reserved <= BIOS_BASE_MEMORY_LIMIT ||
                total_bytes <= top_reserved) {
                return -1;
            }
            e820_set_entry(entry, top_reserved, total_bytes - top_reserved,
                           BIOS_E820_TYPE_RESERVED);
            return 0;
        default:
            return -1;
    }
}
