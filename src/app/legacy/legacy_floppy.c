#include "legacy_floppy.h"
#include "legacy_platform.h"
#include "shared_service/service_table.h"

#define ROM_FREE_DESCRIPTOR_LINEAR \
    (SHARED_ROM_HIGH_BASE + SHARED_ROM_SIZE - 8u)
#define LEGACY_FLOPPY_MAGIC 0x30445346u
#define LEGACY_FLOPPY_HEADER_SIZE 32u
#define LEGACY_FLOPPY_RUN_SIZE 8u
#define LEGACY_FLOPPY_MAX_SECTORS 2880u

static unsigned int legacy_floppy_rom_linear = 0;
static unsigned int legacy_floppy_sectors = 0;
static unsigned short legacy_floppy_runs = 0;

static unsigned char rom_u8(unsigned int linear) {
    return *(volatile unsigned char*)linear;
}

static unsigned short rom_u16(unsigned int linear) {
    return (unsigned short)((unsigned short)rom_u8(linear) |
                            ((unsigned short)rom_u8(linear + 1u) << 8));
}

static unsigned int rom_u32(unsigned int linear) {
    return (unsigned int)rom_u16(linear) |
           ((unsigned int)rom_u16(linear + 2u) << 16);
}

static int range_valid(unsigned int base, unsigned int end, unsigned int off,
                       unsigned int len) {
    unsigned int size = end - base;

    if (off > size || len > size - off) {
        return 0;
    }
    return 1;
}

void legacy_floppy_probe(void) {
    unsigned int base = rom_u32(ROM_FREE_DESCRIPTOR_LINEAR);
    unsigned int end = rom_u32(ROM_FREE_DESCRIPTOR_LINEAR + 4u);
    unsigned int free_size;
    unsigned int crc_block;
    unsigned int crc_count;
    unsigned int crc_table_off;
    unsigned int data_off;
    unsigned int i;

    legacy_floppy_rom_linear = 0;
    legacy_floppy_sectors = 0;
    legacy_floppy_runs = 0;

    if (base >= end || end > ROM_FREE_DESCRIPTOR_LINEAR ||
        base < SHARED_ROM_HIGH_BASE || rom_u32(base) != LEGACY_FLOPPY_MAGIC) {
        return;
    }

    free_size = end - base;
    legacy_floppy_runs = rom_u16(base + 4u);
    legacy_floppy_sectors = rom_u32(base + 8u);
    crc_block = rom_u32(base + 12u);
    crc_count = rom_u32(base + 16u);
    crc_table_off = rom_u32(base + 20u);
    data_off = rom_u32(base + 24u);

    if (legacy_floppy_runs > 1024u ||
        legacy_floppy_sectors < LEGACY_FLOPPY_HEADS * LEGACY_FLOPPY_SPT ||
        legacy_floppy_sectors > LEGACY_FLOPPY_MAX_SECTORS || crc_block == 0u ||
        !range_valid(base, end, 0u, LEGACY_FLOPPY_HEADER_SIZE) ||
        !range_valid(base, end, LEGACY_FLOPPY_HEADER_SIZE,
                     (unsigned int)legacy_floppy_runs *
                         LEGACY_FLOPPY_RUN_SIZE) ||
        !range_valid(base, end, crc_table_off, crc_count * 4u) ||
        data_off > free_size) {
        legacy_floppy_runs = 0;
        legacy_floppy_sectors = 0;
        return;
    }

    for (i = 0; i < legacy_floppy_runs; ++i) {
        unsigned int run = base + LEGACY_FLOPPY_HEADER_SIZE +
                           i * LEGACY_FLOPPY_RUN_SIZE;
        unsigned int lba = rom_u16(run);
        unsigned int count = rom_u16(run + 2u);
        unsigned int payload_off = rom_u32(run + 4u);
        unsigned int payload_len = count * LEGACY_FLOPPY_SECTOR_SIZE;

        if (count == 0u || lba > legacy_floppy_sectors ||
            count > legacy_floppy_sectors - lba || payload_off < data_off ||
            !range_valid(base, end, payload_off, payload_len)) {
            legacy_floppy_runs = 0;
            legacy_floppy_sectors = 0;
            return;
        }
    }

    legacy_floppy_rom_linear = base;
    legacy_serial_write_string("Test floppy @ ");
    legacy_serial_write_hex32(base);
    legacy_serial_write_string("-");
    legacy_serial_write_hex32(end);
    legacy_serial_write_string(" sectors=");
    legacy_serial_write_u32(legacy_floppy_sectors);
    legacy_serial_write_string(" runs=");
    legacy_serial_write_u32(legacy_floppy_runs);
    legacy_serial_write_string("\r\n");
}

int legacy_floppy_present(void) { return legacy_floppy_rom_linear != 0u; }

unsigned int legacy_floppy_sector_count(void) { return legacy_floppy_sectors; }

static int copy_sector(unsigned int lba, unsigned int dest) {
    unsigned int i;
    unsigned int n;

    for (n = 0; n < LEGACY_FLOPPY_SECTOR_SIZE; ++n) {
        *(volatile unsigned char*)(dest + n) = 0u;
    }

    for (i = 0; i < legacy_floppy_runs; ++i) {
        unsigned int run = legacy_floppy_rom_linear +
                           LEGACY_FLOPPY_HEADER_SIZE +
                           i * LEGACY_FLOPPY_RUN_SIZE;
        unsigned int run_lba = rom_u16(run);
        unsigned int count = rom_u16(run + 2u);
        unsigned int payload_off = rom_u32(run + 4u);
        unsigned int src;

        if (lba < run_lba || lba >= run_lba + count) {
            continue;
        }
        src = legacy_floppy_rom_linear + payload_off +
              (lba - run_lba) * LEGACY_FLOPPY_SECTOR_SIZE;
        for (n = 0; n < LEGACY_FLOPPY_SECTOR_SIZE; ++n) {
            *(volatile unsigned char*)(dest + n) = rom_u8(src + n);
        }
        return 0;
    }
    return 0;
}

int legacy_floppy_read_sectors(unsigned int lba, unsigned int count,
                               unsigned int dest) {
    unsigned int i;

    if (!legacy_floppy_present() || count == 0u ||
        lba > legacy_floppy_sectors ||
        count > legacy_floppy_sectors - lba) {
        return -1;
    }
    for (i = 0; i < count; ++i) {
        if (copy_sector(lba + i,
                        dest + i * LEGACY_FLOPPY_SECTOR_SIZE) != 0) {
            return -1;
        }
    }
    return 0;
}
