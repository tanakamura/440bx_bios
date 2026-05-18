#ifndef LEGACY_BDA_H
#define LEGACY_BDA_H

void legacy_bda_init(int floppy_present, int hdd_present,
                     unsigned short base_mem_kb,
                     unsigned short ebda_segment);

#endif
