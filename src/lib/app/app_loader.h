#ifndef APP_LOADER_H
#define APP_LOADER_H

#include "blob.h"

unsigned int app_load_payload(blob_load_fn load, unsigned int total_bytes,
                              unsigned int payload_id, const char* label);

#endif
