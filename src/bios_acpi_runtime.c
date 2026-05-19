#include "bios_acpi_runtime.h"

#include "acpi_tables.h"
#include "bios_io.h"
#include "bios_serial.h"
#include "shared_service/service_table.h"

static void acpi_clear_pm_events(unsigned int pm1_evt, unsigned int gpe0,
                                 unsigned int gpe0_len) {
    unsigned int half;
    unsigned int i;

    if (pm1_evt != 0u) {
        outw((unsigned short)(pm1_evt + 2u), 0x0000u);
        outw((unsigned short)pm1_evt, 0xffffu);
        (void)inw((unsigned short)pm1_evt);
    }

    if (gpe0 == 0u || gpe0_len < 2u) {
        return;
    }

    half = gpe0_len / 2u;
    for (i = 0; i < half; ++i) {
        outb((unsigned short)(gpe0 + half + i), 0x00u);
    }
    for (i = 0; i < half; ++i) {
        outb((unsigned short)(gpe0 + i), 0xffu);
    }
    (void)inb((unsigned short)gpe0);

    serial_write_string("ACPI PM sts=");
    serial_write_hex16(inw((unsigned short)pm1_evt));
    serial_write_string(" en=");
    serial_write_hex16(inw((unsigned short)(pm1_evt + 2u)));
    serial_write_string(" gpe=");
    serial_write_hex8(inb((unsigned short)gpe0));
    serial_write_string("/");
    serial_write_hex8(inb((unsigned short)(gpe0 + half)));
    serial_write_string("\r\n");
}

void bios_acpi_install_for_linux(unsigned int rsdp_linear,
                                 unsigned int pm1_evt,
                                 unsigned int pm1_cnt,
                                 unsigned int gpe0,
                                 unsigned int gpe0_len,
                                 unsigned int flags) {
    unsigned short cnt;

    if (rsdp_linear == 0u) {
        serial_write_string("ACPI tables missing\r\n");
        return;
    }
    serial_write_string("ACPI RSDP=");
    serial_write_hex32(rsdp_linear);
    serial_write_string("\r\n");
    acpi_clear_pm_events(pm1_evt, gpe0, gpe0_len);
    if ((flags & SHARED_BOOT_ACPI_FLAG_ENABLE_SCI) != 0u && pm1_cnt != 0u) {
        cnt = inw((unsigned short)pm1_cnt);
        if ((cnt & ACPI_PM1_CNT_SCI_EN) == 0u) {
            outw((unsigned short)pm1_cnt,
                 (unsigned short)(cnt | ACPI_PM1_CNT_SCI_EN));
            cnt = inw((unsigned short)pm1_cnt);
        }
        serial_write_string("ACPI PM1 cnt=");
        serial_write_hex16(cnt);
        serial_write_string("\r\n");
    }
}
