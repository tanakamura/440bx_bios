#include "app_context_builder.h"

#include "app_boot.h"
#include "app_platform.h"
#include "bios_settings.h"
#include "bios_stage_context.h"
#include "blob.h"

static void fill_nvram_snapshot(struct app_nvram_snapshot* snapshot,
                                const struct bios_settings* settings) {
    unsigned int i;

    *snapshot = (struct app_nvram_snapshot){0};
    snapshot->flags0 = settings->flags0;
    snapshot->boot_priority = settings->boot_priority;
    snapshot->vmlinux_partition = settings->vmlinux_partition;
    snapshot->enable_memtest = settings->enable_memtest;
    snapshot->run_test_blob = settings->run_test_blob;
    for (i = 0u; i < BIOS_NVRAM_CMDLINE_MAX; ++i) {
        snapshot->linux_cmdline_suffix[i] =
            settings->linux_cmdline_suffix[i];
        if (settings->linux_cmdline_suffix[i] == '\0') {
            break;
        }
    }
    snapshot->linux_cmdline_suffix[BIOS_NVRAM_CMDLINE_MAX - 1u] = '\0';
}

struct app_boot_context* stage3_app_context_alloc_and_fill(
    const struct bios_stage_context* stage,
    const struct bios_settings* settings,
    unsigned int app_id) {
    struct app_boot_context* ctx;
    struct app_nvram_snapshot snapshot;

    if (stage == 0 || settings == 0) {
        return 0;
    }
    ctx = app_boot_context_alloc(stage->total_bytes, stage->shared_service);
    if (ctx == 0) {
        return 0;
    }
    app_boot_context_init(ctx, app_id);
    app_platform_init(&ctx->platform);
    app_platform_set_memory(&ctx->platform, stage->total_bytes);
    app_platform_set_pci_snapshot(&ctx->platform,
                                  stage->shared_service->pci_snapshot_ptr);
    app_platform_set_storage_snapshot(
        &ctx->platform, stage->shared_service->storage_snapshot_ptr);
    app_platform_set_acpi(&ctx->platform, stage->rsdp_linear,
                          stage->acpi_pm1_evt, stage->acpi_pm1_cnt,
                          stage->acpi_gpe0, stage->acpi_gpe0_len,
                          stage->acpi_flags, stage->acpi_table_base,
                          stage->acpi_table_size);
    fill_nvram_snapshot(&snapshot, settings);
    app_platform_set_nvram(&ctx->platform, &snapshot);
    ctx->runtime_base = BIOS_LOAD_LINEAR;
    ctx->runtime_size = BIOS_LOAD_CAPACITY;
    ctx->boot_params_linear = 0u;
    ctx->work_linear = APP_SLOT_LOAD_LINEAR;
    ctx->work_size = APP_SLOT_LOAD_CAPACITY;
    return ctx;
}
