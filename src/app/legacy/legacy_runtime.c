#include "app/legacy/legacy_runtime.h"

#include "bios_memory.h"
#include "bios_rtc.h"
#include "bios_serial.h"
#include "bios_storage.h"
#include "app/legacy/legacy_floppy.h"
#include "app/legacy/legacy_platform.h"
#include "app/legacy/legacy_timer.h"

static void legacy_local_hdd_get_geometry(
    struct legacy_hdd_geometry* geometry) {
    struct bios_hdd_geometry bios_geometry;

    bios_hdd_get_geometry(&bios_geometry);
    geometry->total_sectors = bios_geometry.total_sectors;
    geometry->cylinders = bios_geometry.cylinders;
    geometry->heads = bios_geometry.heads;
    geometry->sectors_per_track = bios_geometry.sectors_per_track;
}

static unsigned int
legacy_local_memory_extended_usable_end(unsigned int total_bytes) {
    return bios_memory_extended_usable_end(total_bytes);
}

static unsigned int
legacy_local_memory_e820_entry_count(unsigned int total_bytes) {
    return bios_memory_e820_entry_count(total_bytes);
}

static int legacy_local_memory_e820_get_entry(
    unsigned int total_bytes, unsigned int index,
    struct legacy_e820_entry* entry) {
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

static void legacy_runtime_fill_local_platform_ops(
    struct legacy_platform_ops* ops) {
    *ops = (struct legacy_platform_ops){0};
    ops->serial_write_char = serial_write_char;
    ops->serial_write_string = serial_write_string;
    ops->serial_write_hex8 = serial_write_hex8;
    ops->serial_write_hex16 = serial_write_hex16;
    ops->serial_write_hex32 = serial_write_hex32;
    ops->serial_write_u32 = serial_write_u32;
    ops->hdd_is_present = bios_hdd_is_present;
    ops->hdd_current_kind = bios_hdd_current_kind;
    ops->hdd_select_kind = bios_hdd_select_kind;
    ops->hdd_get_geometry = legacy_local_hdd_get_geometry;
    ops->hdd_read_sectors = bios_hdd_read_sectors;
    ops->hdd_load_mbr_boot_sector = bios_hdd_load_mbr_boot_sector;
    ops->rtc_read_time_bcd = bios_rtc_read_time_bcd;
    ops->rtc_read_date_bcd = bios_rtc_read_date_bcd;
    ops->rtc_set_time_bcd = bios_rtc_set_time_bcd;
    ops->rtc_set_date_bcd = bios_rtc_set_date_bcd;
    ops->memory_extended_usable_end = legacy_local_memory_extended_usable_end;
    ops->memory_e820_entry_count = legacy_local_memory_e820_entry_count;
    ops->memory_e820_get_entry = legacy_local_memory_e820_get_entry;
}

void legacy_runtime_fill_exports(struct legacy_app_exports* exports) {
    if (exports == 0) {
        return;
    }
    exports->magic = LEGACY_APP_EXPORTS_MAGIC;
    exports->version = LEGACY_APP_EXPORTS_VERSION;
    exports->size = sizeof(*exports);
    exports->prepare_boot_sector = legacy_prepare_boot_sector;
    exports->install_boot_drive = legacy_install_boot_drive;
    exports->install_pm_stack_top = legacy_install_pm_stack_top;
}

void legacy_runtime_init(const struct legacy_runtime_config* config) {
    struct legacy_service_context context;
    struct legacy_platform_ops local_ops;
    unsigned int floppy_dpt_linear;

    legacy_runtime_fill_local_platform_ops(&local_ops);
    legacy_platform_init(&local_ops);
    storage_set_scratch_base(bios_memory_top_reserved_base(
        config->total_bytes));
    storage_scan(config->total_bytes);
    legacy_floppy_probe();
    floppy_dpt_linear = legacy_install_bios_thunks(
        legacy_floppy_present(), bios_hdd_is_present(), config->base_mem_kb,
        config->ebda_segment, config->install_shadow, 0);

    context.total_bytes = config->total_bytes;
    context.floppy_dpt_linear = floppy_dpt_linear;
    context.base_mem_kb = config->base_mem_kb;
    context.boot_priority = config->boot_priority;
    context.record_boot_success = config->record_boot_success;
    context.boot_pm32 = config->boot_pm32;
    legacy_service_init(&context);
    legacy_timer_init();
    legacy_runtime_fill_exports(config->exports);
}
