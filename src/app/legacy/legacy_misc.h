#ifndef LEGACY_MISC_H
#define LEGACY_MISC_H

#include "legacy_rm.h"

void legacy_int11_service(struct rm_int13_frame* f);
void legacy_int12_service(struct rm_int13_frame* f, unsigned short base_mem_kb);
void legacy_int17_service(struct rm_int13_frame* f);

#endif
