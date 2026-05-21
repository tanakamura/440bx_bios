#ifndef LEGACY_RUNTIME_H
#define LEGACY_RUNTIME_H

#include "legacy_app_abi.h"

void legacy_runtime_init(const struct legacy_runtime_config* config);
void legacy_runtime_fill_exports(struct legacy_app_exports* exports);

#endif
