#ifndef BIOS_MEMORY_H
#define BIOS_MEMORY_H

#define BIOS_TOP_RESERVED_SIZE 0x00100000u
#define BIOS_E820_TYPE_USABLE 1u
#define BIOS_E820_TYPE_RESERVED 2u

struct e820_entry {
    unsigned int base_low;
    unsigned int base_high;
    unsigned int length_low;
    unsigned int length_high;
    unsigned int type;
} __attribute__((packed));

unsigned int bios_memory_top_reserved_base(unsigned int total_bytes);
unsigned int bios_memory_extended_usable_end(unsigned int total_bytes);
unsigned int bios_memory_e820_entry_count(unsigned int total_bytes);
int bios_memory_e820_get_entry(unsigned int total_bytes, unsigned int index,
                               struct e820_entry* entry);

#endif
