#include "bios_legacy.h"

#include "blob.h"
#include "bios_memory.h"
#include "bios_rtc.h"
#include "bios_serial.h"
#include "bios_stage_context.h"
#include "bios_storage.h"
#include "app/legacy/legacy_boot.h"
#include "app/legacy/legacy_platform.h"
#include "app/legacy/legacy_rm.h"
#include "app/legacy/legacy_runtime.h"
#include "app/legacy/legacy_thunk.h"

static const unsigned short bios_ebda_segment = 0x0000u;
static const unsigned short bios_dos_base_mem_kb = 640u;

#define BIOS_LEGACY_APP_LOAD_FALLBACK 0x000f0000u
#define BIOS_LEGACY_APP_LOAD_CAPACITY 0x00010000u

typedef void (*legacy_app_entry_fn)(const struct legacy_runtime_config* config);

static struct legacy_app_exports bios_legacy_exports;

static void legacy_hdd_get_geometry_cb(struct legacy_hdd_geometry* geometry) {
    struct bios_hdd_geometry bios_geometry;

    bios_hdd_get_geometry(&bios_geometry);
    geometry->total_sectors = bios_geometry.total_sectors;
    geometry->cylinders = bios_geometry.cylinders;
    geometry->heads = bios_geometry.heads;
    geometry->sectors_per_track = bios_geometry.sectors_per_track;
}

static unsigned int
legacy_memory_extended_usable_end_cb(unsigned int total_bytes) {
    return bios_memory_extended_usable_end(total_bytes);
}

static unsigned int legacy_memory_e820_entry_count_cb(unsigned int total_bytes) {
    return bios_memory_e820_entry_count(total_bytes);
}

static int legacy_memory_e820_get_entry_cb(unsigned int total_bytes,
                                           unsigned int index,
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

static void bios_legacy_fill_platform_ops(struct legacy_platform_ops* ops) {
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
    ops->hdd_get_geometry = legacy_hdd_get_geometry_cb;
    ops->hdd_read_sectors = bios_hdd_read_sectors;
    ops->hdd_load_mbr_boot_sector = bios_hdd_load_mbr_boot_sector;
    ops->rtc_read_time_bcd = bios_rtc_read_time_bcd;
    ops->rtc_read_date_bcd = bios_rtc_read_date_bcd;
    ops->rtc_set_time_bcd = bios_rtc_set_time_bcd;
    ops->rtc_set_date_bcd = bios_rtc_set_date_bcd;
    ops->memory_extended_usable_end = legacy_memory_extended_usable_end_cb;
    ops->memory_e820_entry_count = legacy_memory_e820_entry_count_cb;
    ops->memory_e820_get_entry = legacy_memory_e820_get_entry_cb;
}

void bios_legacy_install_runtime(
    const struct bios_stage_context* stage, unsigned char boot_priority,
    bios_legacy_record_boot_success_fn record_boot_success,
    bios_legacy_void_fn boot_pm32, bios_legacy_void_fn install_shadow) {
    struct legacy_runtime_config config;
    blob_load_fn load;

    bios_legacy_fill_platform_ops(&config.platform_ops);
    config.total_bytes = stage->total_bytes;
    config.hdd_present = bios_hdd_is_present();
    config.base_mem_kb = bios_dos_base_mem_kb;
    config.ebda_segment = bios_ebda_segment;
    config.boot_priority = boot_priority;
    config.record_boot_success = record_boot_success;
    config.boot_pm32 = boot_pm32;
    config.install_shadow = install_shadow;
    config.exports = &bios_legacy_exports;
    bios_legacy_exports = (struct legacy_app_exports){0};

    load = bios_stage_context_blob_load(stage);
    if (stage->legacy_app_blob_linear != 0u && load != 0) {
        struct blob_status status;
        unsigned int load_addr = BIOS_LEGACY_APP_LOAD_FALLBACK;
        int rc = load(SHARED_PAYLOAD_ID_LEGACY_APP,
                      (void*)BIOS_LEGACY_APP_LOAD_FALLBACK,
                      BIOS_LEGACY_APP_LOAD_CAPACITY, &load_addr, &status,
                      stage->total_bytes);
        if (rc == 0) {
            serial_write_string("Legacy app @ ");
            serial_write_hex32(load_addr);
            serial_write_string("\r\n");
            ((legacy_app_entry_fn)load_addr)(&config);
            return;
        }
        serial_write_string("Legacy app load failed rc=");
        serial_write_hex32((unsigned int)rc);
        serial_write_string("\r\n");
    }

    serial_write_string("Legacy app missing; direct thunk only\r\n");
    legacy_install_bios_thunks(0, config.hdd_present, config.base_mem_kb,
                               config.ebda_segment, install_shadow, 0);
}

static int legacy_exports_ready(void) {
    return bios_legacy_exports.magic == LEGACY_APP_EXPORTS_MAGIC &&
           bios_legacy_exports.version == LEGACY_APP_EXPORTS_VERSION &&
           bios_legacy_exports.size >= sizeof(bios_legacy_exports);
}

unsigned char bios_legacy_prepare_boot_sector(
    unsigned char boot_priority,
    bios_legacy_record_boot_success_fn record_boot_success) {
    if (legacy_exports_ready() &&
        bios_legacy_exports.prepare_boot_sector != 0) {
        return bios_legacy_exports.prepare_boot_sector(boot_priority,
                                                       record_boot_success);
    }
    serial_write_string("Legacy app missing for boot\r\n");
    for (;;) {
        __asm__ volatile("hlt");
    }
}

void bios_legacy_install_boot_drive(unsigned char boot_drive) {
    if (legacy_exports_ready() && bios_legacy_exports.install_boot_drive != 0) {
        bios_legacy_exports.install_boot_drive(boot_drive);
        return;
    }
    legacy_install_boot_drive(boot_drive);
}

void bios_legacy_install_pm_stack_top(unsigned int pm_stack_top) {
    if (legacy_exports_ready() &&
        bios_legacy_exports.install_pm_stack_top != 0) {
        bios_legacy_exports.install_pm_stack_top(pm_stack_top);
        return;
    }
    legacy_install_pm_stack_top(pm_stack_top);
}

void bios_rm_service(unsigned int vector, struct rm_int13_frame* f) {
    (void)vector;
    if (f != 0) {
        f->flags |= 0x0001u;
    }
}
