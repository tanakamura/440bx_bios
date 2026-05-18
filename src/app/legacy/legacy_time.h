#ifndef LEGACY_TIME_H
#define LEGACY_TIME_H

#include "app/legacy/legacy_rm.h"

void legacy_int1a_service(struct rm_int13_frame* f,
                          unsigned int* tick_counter);

#endif
