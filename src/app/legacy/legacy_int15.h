#ifndef LEGACY_INT15_H
#define LEGACY_INT15_H

#include "legacy_rm.h"

void legacy_int15_service(struct rm_int13_frame* f, unsigned int total_bytes);

#endif
