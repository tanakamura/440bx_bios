#ifndef LEGACY_INT13_H
#define LEGACY_INT13_H

#include "legacy_rm.h"

void legacy_int13_service(struct rm_int13_frame* f,
                          unsigned int floppy_dpt_linear);

#endif
