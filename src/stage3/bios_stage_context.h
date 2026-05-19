#ifndef BIOS_STAGE_CONTEXT_H
#define BIOS_STAGE_CONTEXT_H

#include "blob.h"
#include "shared_service/service_table.h"

struct bios_stage_context {
    unsigned int total_bytes;
    unsigned int vgabios_blob_linear;
    unsigned int linux_loader_blob_linear;
    unsigned int test_elf_blob_linear;
    unsigned int rsdp_linear;
    unsigned int acpi_pm1_evt;
    unsigned int acpi_pm1_cnt;
    unsigned int acpi_gpe0;
    unsigned int acpi_gpe0_len;
    unsigned int acpi_flags;
    struct shared_service_table* shared_service;
    unsigned char maintenance_requested;
    unsigned char shadow_ready;
};

void bios_stage_context_load(struct bios_stage_context* context,
                             unsigned int total_bytes);
unsigned int bios_stage_context_payload_blob(
    const struct bios_stage_context* context, unsigned int payload_id);
blob_expand_fn bios_stage_context_blob_expand(
    const struct bios_stage_context* context);
blob_load_fn bios_stage_context_blob_load(
    const struct bios_stage_context* context);
void* bios_stage_context_blob_stage(const struct bios_stage_context* context);

#endif
