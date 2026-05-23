#ifndef BIOS_STORAGE_H
#define BIOS_STORAGE_H

struct bios_hdd_geometry {
    unsigned int total_sectors;
    unsigned short cylinders;
    unsigned short heads;
    unsigned short sectors_per_track;
};

#define BIOS_HDD_KIND_NONE 0u
#define BIOS_HDD_KIND_IDE 1u
#define BIOS_HDD_KIND_USB 2u

#include "shared_service/service_table.h"

void storage_scan(unsigned int total_bytes);
void storage_set_scratch_base(unsigned int base);
int storage_snapshot_export(unsigned int total_bytes,
                            struct shared_service_table* shared);
int storage_snapshot_import(const struct shared_storage_snapshot* snapshot);
unsigned char bios_hdd_is_present(void);
unsigned char bios_hdd_current_kind(void);
unsigned char bios_hdd_select_kind(unsigned char kind);
void bios_hdd_get_dma_caps(unsigned char* dma_enabled,
                           unsigned char* lba48_dma_enabled);
void bios_hdd_set_dma_caps(unsigned char dma_enabled,
                           unsigned char lba48_dma_enabled);
int bios_hdd_force_pio4(void);
void bios_hdd_get_geometry(struct bios_hdd_geometry* geometry);
int bios_hdd_read_sectors(unsigned int lba, unsigned int count,
                          unsigned int dest);
int bios_hdd_load_mbr_boot_sector(unsigned int dest);

#endif
