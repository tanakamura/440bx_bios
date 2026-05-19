#include "bios_maintenance.h"

#include "bios_io.h"
#include "bios_nvram.h"
#include "bios_serial.h"

static unsigned char serial_read_char_blocking(void) {
    while ((inb(0x03f8 + 5u) & 0x01u) == 0) {
    }
    return inb(0x03f8);
}

static unsigned int maintenance_read_line(char* buf, unsigned int cap) {
    unsigned int len = 0;

    for (;;) {
        unsigned char ch = serial_read_char_blocking();
        if (ch == '\r' || ch == '\n') {
            serial_write_string("\r\n");
            break;
        }
        if (ch == 0x08u || ch == 0x7fu) {
            if (len != 0u) {
                --len;
                serial_write_string("\b \b");
            }
            continue;
        }
        if (ch < 0x20u || ch >= 0x7fu) {
            continue;
        }
        if (len + 1u < cap) {
            buf[len++] = (char)ch;
            serial_write_char((char)ch);
        }
    }
    buf[len] = '\0';
    return len;
}

static void maintenance_print_settings(
    const struct bios_maintenance_config* config) {
    unsigned char flags0 = *config->flags0;

    serial_write_string("vmlinux partition=");
    serial_write_u32(*config->vmlinux_partition);
    serial_write_string("\r\nflags0=");
    serial_write_hex8(flags0);
    serial_write_string(" vesa=");
    serial_write_u32((flags0 & BIOS_NVRAM_FLAGS0_VESA_1024_768) != 0u);
    serial_write_string(" serial=");
    serial_write_u32((flags0 & BIOS_NVRAM_FLAGS0_SERIAL_CONSOLE) != 0u);
    serial_write_string(" memtest=");
    serial_write_u32(*config->enable_memtest);
    serial_write_string(" testblob=");
    serial_write_u32(*config->run_test_blob);
    serial_write_string("\r\nboot priority=");
    serial_write_u32(*config->boot_priority);
    serial_write_string(" (0=auto 1=ide 2=usb)\r\ncmdline='");
    serial_write_string(config->linux_cmdline_suffix);
    serial_write_string("'\r\n");
}

static void maintenance_set_cmdline(
    const struct bios_maintenance_config* config, const char* text) {
    unsigned int i;

    for (i = 0; i < BIOS_NVRAM_CMDLINE_MAX - 1u && text[i] != '\0'; ++i) {
        config->linux_cmdline_suffix[i] = text[i];
    }
    config->linux_cmdline_suffix[i] = '\0';
    if (config->save_cmdline_suffix != 0) {
        config->save_cmdline_suffix(config->linux_cmdline_suffix);
    }
    serial_write_string("cmdline saved\r\n");
}

static void maintenance_set_partition(
    const struct bios_maintenance_config* config, char ch) {
    if (ch != '0' && ch != '1') {
        serial_write_string("usage: b <0|1>\r\n");
        return;
    }
    *config->vmlinux_partition = (unsigned char)(ch - '0');
    if (config->save_partition != 0) {
        config->save_partition(*config->vmlinux_partition);
    }
    serial_write_string("partition saved\r\n");
}

static void maintenance_set_flag(const struct bios_maintenance_config* config,
                                 char ch, unsigned char bit,
                                 const char* name) {
    if (ch != '0' && ch != '1') {
        serial_write_string("usage: flag <0|1>\r\n");
        return;
    }
    if (ch == '1') {
        *config->flags0 = (unsigned char)(*config->flags0 | bit);
    } else {
        *config->flags0 = (unsigned char)(*config->flags0 & ~bit);
    }
    *config->enable_memtest =
        (unsigned char)((*config->flags0 & BIOS_NVRAM_FLAGS0_MEMTEST) != 0u);
    *config->run_test_blob = (unsigned char)(
        (*config->flags0 & BIOS_NVRAM_FLAGS0_RUN_TEST_BLOB) != 0u);
    if (config->save_flags0 != 0) {
        config->save_flags0(*config->flags0);
    }
    serial_write_string(name);
    serial_write_string(" saved\r\n");
}

static void maintenance_set_boot_priority(
    const struct bios_maintenance_config* config, char ch) {
    unsigned char priority;
    if (ch < '0' || ch > '2') {
        serial_write_string("usage: o <0|1|2>\r\n");
        return;
    }
    priority = (unsigned char)(ch - '0');
    *config->boot_priority = priority;
    if (config->save_boot_priority != 0) {
        config->save_boot_priority(priority);
    }
    serial_write_string("boot priority saved\r\n");
}

static void maintenance_reset_defaults(
    const struct bios_maintenance_config* config) {
    if (config->reset_defaults != 0) {
        config->reset_defaults();
    }
    *config->flags0 = BIOS_NVRAM_FLAGS0_DEFAULT;
    *config->boot_priority = BIOS_NVRAM_BOOT_PRIORITY_DEFAULT;
    *config->vmlinux_partition = 0u;
    *config->enable_memtest = 0u;
    *config->run_test_blob = 0u;
    config->linux_cmdline_suffix[0] = '\0';
    serial_write_string("defaults saved\r\n");
}

void bios_maintenance_prompt(const struct bios_maintenance_config* config) {
    char line[BIOS_NVRAM_CMDLINE_MAX + 8u];

    serial_write_string("\r\nMaintenance mode\r\n");
    serial_write_string(
        "commands: a <cmdline>, b <0|1>, m <0|1>, t <0|1>, s <0|1>, v <0|1>, o <0|1|2>, d, p, q\r\n");
    maintenance_print_settings(config);
    for (;;) {
        serial_write_string("M> ");
        maintenance_read_line(line, sizeof(line));
        if (line[0] == '\0') {
            continue;
        }
        if (line[0] == 'q' && line[1] == '\0') {
            serial_write_string("boot\r\n");
            return;
        }
        if (line[0] == 'p' && line[1] == '\0') {
            maintenance_print_settings(config);
            continue;
        }
        if (line[0] == 'd' && line[1] == '\0') {
            maintenance_reset_defaults(config);
            continue;
        }
        if (line[0] == 'a' && line[1] == ' ') {
            maintenance_set_cmdline(config, line + 2);
            continue;
        }
        if (line[0] == 'a' && line[1] == '\0') {
            maintenance_set_cmdline(config, "");
            continue;
        }
        if (line[0] == 'b' && line[1] == ' ' && line[2] != '\0' &&
            line[3] == '\0') {
            maintenance_set_partition(config, line[2]);
            continue;
        }
        if (line[0] == 'm' && line[1] == ' ' && line[2] != '\0' &&
            line[3] == '\0') {
            maintenance_set_flag(config, line[2], BIOS_NVRAM_FLAGS0_MEMTEST,
                                 "memtest");
            continue;
        }
        if (line[0] == 't' && line[1] == ' ' && line[2] != '\0' &&
            line[3] == '\0') {
            maintenance_set_flag(config, line[2],
                                 BIOS_NVRAM_FLAGS0_RUN_TEST_BLOB,
                                 "test blob");
            continue;
        }
        if (line[0] == 's' && line[1] == ' ' && line[2] != '\0' &&
            line[3] == '\0') {
            maintenance_set_flag(config, line[2],
                                 BIOS_NVRAM_FLAGS0_SERIAL_CONSOLE,
                                 "serial console");
            continue;
        }
        if (line[0] == 'v' && line[1] == ' ' && line[2] != '\0' &&
            line[3] == '\0') {
            maintenance_set_flag(config, line[2],
                                 BIOS_NVRAM_FLAGS0_VESA_1024_768, "vesa");
            continue;
        }
        if (line[0] == 'o' && line[1] == ' ' && line[2] != '\0' &&
            line[3] == '\0') {
            maintenance_set_boot_priority(config, line[2]);
            continue;
        }
        serial_write_string("?\r\n");
    }
}
