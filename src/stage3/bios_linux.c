#include "bios_linux.h"

#include "app/legacy/legacy_thunk.h"
#include "blob.h"
#include "bios_acpi_runtime.h"
#include "bios_memory.h"
#include "bios_nvram.h"
#include "bios_rtc.h"
#include "bios_serial.h"
#include "bios_storage.h"

static struct bios_linux_config active_config;

#define BIOS_LINUX_APP_LOAD_FALLBACK 0x000f0000u
#define BIOS_LINUX_APP_LOAD_CAPACITY 0x00010000u

typedef int (*linux_loader_app_entry_fn)(
    const struct linux_loader_config* loader);

static void linux_hdd_get_geometry_cb(
    struct linux_loader_hdd_geometry* geometry) {
    struct bios_hdd_geometry bios_geometry;

    bios_hdd_get_geometry(&bios_geometry);
    geometry->total_sectors = bios_geometry.total_sectors;
    geometry->cylinders = bios_geometry.cylinders;
    geometry->heads = bios_geometry.heads;
    geometry->sectors_per_track = bios_geometry.sectors_per_track;
}

static int linux_memory_e820_get_entry_cb(unsigned int total_bytes,
                                          unsigned int index,
                                          struct linux_loader_e820_entry* entry) {
    struct e820_entry bios_entry;

    if (bios_memory_e820_get_entry(total_bytes, index, &bios_entry) != 0) {
        return -1;
    }
    entry->base_low = bios_entry.base_low;
    entry->base_high = bios_entry.base_high;
    entry->length_low = bios_entry.length_low;
    entry->length_high = bios_entry.length_high;
    entry->type = bios_entry.type;
    return 0;
}

static void prepare_linux_platform(void) {
    const struct bios_stage_context* stage = active_config.stage;

    bios_rtc_prepare_for_linux(bios_nvram_enable_extended_cmos);
    bios_acpi_install_for_linux(
        stage->rsdp_linear, stage->acpi_pm1_evt, stage->acpi_pm1_cnt,
        stage->acpi_gpe0, stage->acpi_gpe0_len, stage->acpi_flags);
}

void bios_linux_fill_loader_config(struct linux_loader_config* loader,
                                   const struct bios_linux_config* config) {
    const struct bios_settings* settings = config->settings;

    active_config = *config;
    loader->total_bytes = config->stage->total_bytes;
    loader->boot_priority = settings->boot_priority;
    loader->vmlinux_partition = settings->vmlinux_partition;
    loader->enable_serial_console =
        (settings->flags0 & BIOS_NVRAM_FLAGS0_SERIAL_CONSOLE) != 0u ? 1u : 0u;
    loader->enable_vesa_1024_768 =
        (settings->flags0 & BIOS_NVRAM_FLAGS0_VESA_1024_768) != 0u ? 1u : 0u;
    loader->cmdline_suffix = settings->linux_cmdline_suffix;
    loader->prepare_platform = prepare_linux_platform;
    loader->init_vgabios = config->init_vgabios;
    loader->record_boot_success = config->record_boot_success;
    loader->vbe_mode_info_buffer = legacy_vbe_mode_info_buffer;
    loader->vbe_mode_info_pm32 = config->vbe_mode_info_pm32;
    loader->vbe_set_mode_pm32 = config->vbe_set_mode_pm32;
    loader->serial_write_string = serial_write_string;
    loader->serial_write_hex8 = serial_write_hex8;
    loader->serial_write_hex16 = serial_write_hex16;
    loader->serial_write_hex32 = serial_write_hex32;
    loader->serial_write_u32 = serial_write_u32;
    loader->hdd_is_present = bios_hdd_is_present;
    loader->hdd_current_kind = bios_hdd_current_kind;
    loader->hdd_select_kind = bios_hdd_select_kind;
    loader->hdd_get_geometry = linux_hdd_get_geometry_cb;
    loader->hdd_read_sectors = bios_hdd_read_sectors;
    loader->memory_extended_usable_end = bios_memory_extended_usable_end;
    loader->memory_e820_entry_count = bios_memory_e820_entry_count;
    loader->memory_e820_get_entry = linux_memory_e820_get_entry_cb;
}

int bios_linux_try_boot(const struct bios_linux_config* config) {
    struct linux_loader_config loader = {0};
    blob_load_fn load;

    bios_linux_fill_loader_config(&loader, config);
    if (config->stage->linux_loader_blob_linear == 0u) {
        return 0;
    }

    load = bios_stage_context_blob_load(config->stage);
    if (load != 0) {
        struct blob_status status;
        unsigned int load_addr = BIOS_LINUX_APP_LOAD_FALLBACK;
        int rc = load(SHARED_PAYLOAD_ID_LINUX_LOADER_APP,
                      (void*)BIOS_LINUX_APP_LOAD_FALLBACK,
                      BIOS_LINUX_APP_LOAD_CAPACITY, &load_addr, &status,
                      config->stage->total_bytes);
        if (rc == 0) {
            serial_write_string("Linux app @ ");
            serial_write_hex32(load_addr);
            serial_write_string("\r\n");
            return ((linux_loader_app_entry_fn)load_addr)(&loader);
        }
        serial_write_string("Linux app load failed rc=");
        serial_write_hex32((unsigned int)rc);
        serial_write_string("\r\n");
    }
    return 0;
}
