#include "bios_stage_context.h"

static unsigned int payload_blob_ptr(struct shared_service_table* service,
                                     unsigned int payload_id) {
    struct shared_payload_entry* payload = shared_payload_find(service,
                                                               payload_id);

    if (payload == 0) {
        return 0u;
    }
    return payload->blob_ptr;
}

void bios_stage_context_load(struct bios_stage_context* context,
                             unsigned int total_bytes) {
    struct shared_boot_context* boot_ctx;
    unsigned int blob;

    context->total_bytes = total_bytes;
    context->vgabios_blob_linear = 0u;
    context->legacy_app_blob_linear = 0u;
    context->linux_loader_blob_linear = 0u;
    context->test_elf_payload_linear = 0u;
    context->rsdp_linear = 0u;
    context->acpi_pm1_evt = 0u;
    context->acpi_pm1_cnt = 0u;
    context->acpi_gpe0 = 0u;
    context->acpi_gpe0_len = 0u;
    context->acpi_flags = 0u;
    context->maintenance_requested = 0u;
    context->shadow_ready = 0u;
    context->shared_service = shared_service_from_total(total_bytes);

    boot_ctx = shared_boot_context(context->shared_service);
    if (boot_ctx != 0) {
        if ((boot_ctx->flags & SHARED_BOOT_FLAG_MAINTENANCE_REQUESTED) != 0u) {
            context->maintenance_requested = 1u;
        }
        if ((boot_ctx->flags & SHARED_BOOT_FLAG_SHADOW_READY) != 0u) {
            context->shadow_ready = 1u;
        }
        context->rsdp_linear = boot_ctx->rsdp_linear;
        context->acpi_pm1_evt = boot_ctx->acpi_pm1_evt;
        context->acpi_pm1_cnt = boot_ctx->acpi_pm1_cnt;
        context->acpi_gpe0 = boot_ctx->acpi_gpe0;
        context->acpi_gpe0_len = boot_ctx->acpi_gpe0_len;
        context->acpi_flags = boot_ctx->acpi_flags;
    }

    blob = payload_blob_ptr(context->shared_service, SHARED_PAYLOAD_ID_VGABIOS);
    if (blob != 0u) {
        context->vgabios_blob_linear = blob;
    }
    blob = payload_blob_ptr(context->shared_service,
                            SHARED_PAYLOAD_ID_LEGACY_APP);
    if (blob != 0u) {
        context->legacy_app_blob_linear = blob;
    }
    blob = payload_blob_ptr(context->shared_service,
                            SHARED_PAYLOAD_ID_LINUX_LOADER_APP);
    if (blob != 0u) {
        context->linux_loader_blob_linear = blob;
    }
    blob = payload_blob_ptr(context->shared_service, SHARED_PAYLOAD_ID_TEST_ELF);
    if (blob != 0u) {
        context->test_elf_payload_linear = blob;
    }
}

unsigned int bios_stage_context_payload_blob(
    const struct bios_stage_context* context, unsigned int payload_id) {
    if (context == 0) {
        return 0u;
    }
    return payload_blob_ptr(context->shared_service, payload_id);
}

blob_load_fn bios_stage_context_blob_load(
    const struct bios_stage_context* context) {
    if (context->shared_service != 0 &&
        context->shared_service->blob_load != 0u) {
        return (blob_load_fn)context->shared_service->blob_load;
    }
    return 0;
}

void bios_stage_context_release_shared_service(
    const struct bios_stage_context* context) {
    volatile unsigned int* slot;

    if (context == 0 || context->total_bytes < 0x1000u) {
        return;
    }

    slot = (volatile unsigned int*)shared_table_pointer_slot(
        context->total_bytes);
    if (*slot == (unsigned int)context->shared_service) {
        *slot = 0u;
    }
}
