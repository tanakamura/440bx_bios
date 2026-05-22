#ifndef APP_BOOT_H
#define APP_BOOT_H

#include "app_boot_abi.h"
#include "blob.h"
#include "shared_service/service_table.h"

struct app_boot_context* app_boot_context_alloc(unsigned int total_bytes,
                                                struct shared_service_table* shared);
void app_boot_context_init(struct app_boot_context* ctx, unsigned int app_id);
int app_boot_run(blob_load_fn load, unsigned int total_bytes,
                 unsigned int payload_id, const char* label,
                 const struct app_boot_context* ctx);

#endif
