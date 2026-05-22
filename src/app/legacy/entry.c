#include "app/legacy/legacy_runtime.h"

#include "app/legacy/legacy_boot.h"
#include "bios_serial.h"

extern void bios_boot_freedos_pm32(void);

int app_entry(const struct app_boot_context* ctx) {
    unsigned char boot_drive;

    legacy_runtime_init(ctx);
    boot_drive = legacy_prepare_boot_sector();
    serial_write_string("Booting drive=");
    serial_write_hex8(boot_drive);
    serial_write_string("...\r\n");
    bios_boot_freedos_pm32();
    return APP_BOOT_RESULT_DONE;
}
