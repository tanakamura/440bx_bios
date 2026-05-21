#include "bios_settings.h"

#include "bios_maintenance.h"
#include "bios_serial.h"
#include "bios_storage.h"

static void save_partition(unsigned char part) {
    bios_nvram_save_partition(part);
}

static void save_flags0(unsigned char flags0) {
    bios_nvram_save_flags0(flags0);
}

static void save_boot_priority(unsigned char priority) {
    bios_nvram_save_boot_priority(priority);
}

static void save_cmdline_suffix(const char* text) {
    bios_nvram_save_cmdline_suffix(text);
}

static void reset_defaults(void) {
    if (bios_nvram_enable_extended_cmos() == 0) {
        bios_nvram_init_defaults();
    }
}

void bios_settings_fill_shared_snapshot(const struct bios_settings* settings,
                                        struct shared_boot_context* boot_ctx) {
    unsigned int i;

    if (settings == 0 || boot_ctx == 0) {
        return;
    }
    boot_ctx->nvram.magic = BIOS_NVRAM_MAGIC;
    boot_ctx->nvram.flags0 = settings->flags0;
    boot_ctx->nvram.boot_priority = settings->boot_priority;
    boot_ctx->nvram.vmlinux_partition = settings->vmlinux_partition;
    boot_ctx->nvram.enable_memtest = settings->enable_memtest;
    boot_ctx->nvram.run_test_blob = settings->run_test_blob;
    for (i = 0u; i < BIOS_NVRAM_CMDLINE_MAX; ++i) {
        boot_ctx->nvram.linux_cmdline_suffix[i] =
            settings->linux_cmdline_suffix[i];
        if (settings->linux_cmdline_suffix[i] == '\0') {
            break;
        }
    }
    boot_ctx->nvram.linux_cmdline_suffix[BIOS_NVRAM_CMDLINE_MAX - 1u] = '\0';
}

void bios_settings_load(struct bios_settings* settings,
                        struct shared_boot_context* boot_ctx) {
    unsigned int i;
    struct bios_nvram_settings nvram;

    if (boot_ctx != 0 && boot_ctx->nvram.magic == BIOS_NVRAM_MAGIC) {
        settings->flags0 = boot_ctx->nvram.flags0;
        settings->boot_priority = boot_ctx->nvram.boot_priority;
        settings->vmlinux_partition = boot_ctx->nvram.vmlinux_partition;
        settings->enable_memtest = boot_ctx->nvram.enable_memtest;
        settings->run_test_blob = boot_ctx->nvram.run_test_blob;
        for (i = 0u; i < BIOS_NVRAM_CMDLINE_MAX; ++i) {
            settings->linux_cmdline_suffix[i] =
                boot_ctx->nvram.linux_cmdline_suffix[i];
            if (boot_ctx->nvram.linux_cmdline_suffix[i] == '\0') {
                break;
            }
        }
        settings->linux_cmdline_suffix[BIOS_NVRAM_CMDLINE_MAX - 1u] = '\0';
        return;
    }

    bios_nvram_load_settings(&nvram);
    settings->flags0 = nvram.flags0;
    settings->boot_priority = nvram.boot_priority;
    settings->vmlinux_partition = nvram.vmlinux_partition;
    settings->enable_memtest = nvram.enable_memtest;
    settings->run_test_blob = nvram.run_test_blob;
    for (i = 0u; i < BIOS_NVRAM_CMDLINE_MAX; ++i) {
        settings->linux_cmdline_suffix[i] = nvram.linux_cmdline_suffix[i];
        if (nvram.linux_cmdline_suffix[i] == '\0') {
            break;
        }
    }
    settings->linux_cmdline_suffix[BIOS_NVRAM_CMDLINE_MAX - 1u] = '\0';
    bios_settings_fill_shared_snapshot(settings, boot_ctx);
}

void bios_settings_consume_test_blob_request(struct bios_settings* settings) {
    if ((settings->flags0 & BIOS_NVRAM_FLAGS0_RUN_TEST_BLOB) == 0u) {
        return;
    }
    settings->flags0 =
        (unsigned char)(settings->flags0 & ~BIOS_NVRAM_FLAGS0_RUN_TEST_BLOB);
    settings->run_test_blob = 0u;
    bios_nvram_save_flags0(settings->flags0);
    serial_write_string("Test blob request consumed\r\n");
}

void bios_settings_maintenance_prompt(struct bios_settings* settings) {
    struct bios_maintenance_config config;

    config.flags0 = &settings->flags0;
    config.boot_priority = &settings->boot_priority;
    config.vmlinux_partition = &settings->vmlinux_partition;
    config.enable_memtest = &settings->enable_memtest;
    config.run_test_blob = &settings->run_test_blob;
    config.linux_cmdline_suffix = settings->linux_cmdline_suffix;
    config.save_partition = save_partition;
    config.save_flags0 = save_flags0;
    config.save_boot_priority = save_boot_priority;
    config.save_cmdline_suffix = save_cmdline_suffix;
    config.reset_defaults = reset_defaults;
    bios_maintenance_prompt(&config);
}

void bios_settings_record_boot_success(struct bios_settings* settings,
                                       unsigned char kind) {
    if (settings->boot_priority != BIOS_NVRAM_BOOT_PRIORITY_AUTO) {
        return;
    }
    if (kind != BIOS_HDD_KIND_IDE && kind != BIOS_HDD_KIND_USB) {
        return;
    }
    settings->boot_priority = kind;
    bios_nvram_save_boot_priority(kind);
    serial_write_string("boot priority learned=");
    serial_write_u32(kind);
    serial_write_string("\r\n");
}
