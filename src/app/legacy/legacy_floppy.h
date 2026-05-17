#ifndef LEGACY_FLOPPY_H
#define LEGACY_FLOPPY_H

#define LEGACY_FLOPPY_SPT 18u
#define LEGACY_FLOPPY_HEADS 2u
#define LEGACY_FLOPPY_SECTOR_SIZE 512u

void legacy_floppy_probe(void);
int legacy_floppy_present(void);
unsigned int legacy_floppy_sector_count(void);
int legacy_floppy_read_sectors(unsigned int lba, unsigned int count,
                               unsigned int dest);

#endif
