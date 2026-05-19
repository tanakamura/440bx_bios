#ifndef BIOS_MEMTEST_H
#define BIOS_MEMTEST_H

struct shared_service_table;

void bios_memtest_run_optional(unsigned char enable, unsigned int total_bytes,
                               const struct shared_service_table* service);

#endif
