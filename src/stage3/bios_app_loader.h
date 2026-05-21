#ifndef BIOS_APP_LOADER_H
#define BIOS_APP_LOADER_H

#include "bios_stage_context.h"

unsigned int bios_app_load(const struct bios_stage_context* stage,
                           unsigned int payload_id, const char* label);

#endif
