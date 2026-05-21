#ifndef APP_LOADER_H
#define APP_LOADER_H

#include "bios_stage_context.h"

unsigned int app_load_payload(const struct bios_stage_context* stage,
                              unsigned int payload_id, const char* label);

#endif
