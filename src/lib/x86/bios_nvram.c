#include "bios_nvram.h"

#include "bios_io.h"
#include "bios_pci.h"
#include "bios_serial.h"

#define PIIX4_ISA_DEV 7u
#define PIIX4_ISA_FN 0u
#define PIIX4_RTCCFG 0xcbu
#define PIIX4_RTCCFG_RTC_ENABLE 0x01u
#define PIIX4_RTCCFG_UPPER_RAM_EN 0x04u

static int nvram_piix4e_present(void) {
    return pci_read16(0, PIIX4_ISA_DEV, PIIX4_ISA_FN, 0x00u) == 0x8086u;
}

int bios_nvram_enable_extended_cmos(void) {
    unsigned char rtccfg;

    if (!nvram_piix4e_present()) {
        return -1;
    }
    rtccfg = pci_read8(0, PIIX4_ISA_DEV, PIIX4_ISA_FN, PIIX4_RTCCFG);
    if ((rtccfg & (PIIX4_RTCCFG_RTC_ENABLE | PIIX4_RTCCFG_UPPER_RAM_EN)) !=
        (PIIX4_RTCCFG_RTC_ENABLE | PIIX4_RTCCFG_UPPER_RAM_EN)) {
        rtccfg = (unsigned char)(rtccfg | PIIX4_RTCCFG_RTC_ENABLE |
                                 PIIX4_RTCCFG_UPPER_RAM_EN);
        pci_write8(0, PIIX4_ISA_DEV, PIIX4_ISA_FN, PIIX4_RTCCFG, rtccfg);
    }
    return 0;
}

static unsigned char nvram_read(unsigned char index) {
    outb(0x0072u, index);
    return inb(0x0073u);
}

static void nvram_write(unsigned char index, unsigned char value) {
    outb(0x0072u, index);
    outb(0x0073u, value);
}

static unsigned int nvram_read32(unsigned char index) {
    return (unsigned int)nvram_read(index) |
           ((unsigned int)nvram_read((unsigned char)(index + 1u)) << 8) |
           ((unsigned int)nvram_read((unsigned char)(index + 2u)) << 16) |
           ((unsigned int)nvram_read((unsigned char)(index + 3u)) << 24);
}

static void nvram_write32(unsigned char index, unsigned int value) {
    nvram_write(index, (unsigned char)value);
    nvram_write((unsigned char)(index + 1u), (unsigned char)(value >> 8));
    nvram_write((unsigned char)(index + 2u), (unsigned char)(value >> 16));
    nvram_write((unsigned char)(index + 3u), (unsigned char)(value >> 24));
}

static void settings_defaults(struct bios_nvram_settings* settings) {
    settings->flags0 = BIOS_NVRAM_FLAGS0_DEFAULT;
    settings->boot_priority = BIOS_NVRAM_BOOT_PRIORITY_DEFAULT;
    settings->vmlinux_partition = 0u;
    settings->enable_memtest = 0u;
    settings->run_test_blob = 0u;
    settings->linux_cmdline_suffix[0] = '\0';
}

void bios_nvram_init_defaults(void) {
    unsigned int i;

    nvram_write32(0u, BIOS_NVRAM_MAGIC);
    nvram_write(BIOS_NVRAM_PARTITION_OFF, 0u);
    nvram_write(BIOS_NVRAM_FLAGS0_OFF, BIOS_NVRAM_FLAGS0_DEFAULT);
    nvram_write(BIOS_NVRAM_BOOT_PRIORITY_OFF, BIOS_NVRAM_BOOT_PRIORITY_DEFAULT);
    for (i = BIOS_NVRAM_CMDLINE_OFF; i < BIOS_NVRAM_SIZE; ++i) {
        nvram_write((unsigned char)i, 0u);
    }
}

void bios_nvram_load_settings(struct bios_nvram_settings* settings) {
    unsigned int i;
    unsigned char flags0;
    unsigned char terminated = 0;

    settings_defaults(settings);

    if (bios_nvram_enable_extended_cmos() != 0) {
        return;
    }
    if (nvram_read32(0u) != BIOS_NVRAM_MAGIC) {
        serial_write_string("NVRAM init\r\n");
        bios_nvram_init_defaults();
    }

    settings->vmlinux_partition = nvram_read(BIOS_NVRAM_PARTITION_OFF);
    if (settings->vmlinux_partition > 1u) {
        settings->vmlinux_partition = 0u;
        nvram_write(BIOS_NVRAM_PARTITION_OFF, settings->vmlinux_partition);
    }

    flags0 = nvram_read(BIOS_NVRAM_FLAGS0_OFF);
    if ((flags0 & ~BIOS_NVRAM_FLAGS0_KNOWN_MASK) != 0u) {
        flags0 = BIOS_NVRAM_FLAGS0_DEFAULT;
        nvram_write(BIOS_NVRAM_FLAGS0_OFF, flags0);
    }
    settings->flags0 = flags0;
    settings->enable_memtest =
        (unsigned char)((flags0 & BIOS_NVRAM_FLAGS0_MEMTEST) != 0u);
    settings->run_test_blob =
        (unsigned char)((flags0 & BIOS_NVRAM_FLAGS0_RUN_TEST_BLOB) != 0u);

    settings->boot_priority = nvram_read(BIOS_NVRAM_BOOT_PRIORITY_OFF);
    if (settings->boot_priority > BIOS_NVRAM_BOOT_PRIORITY_USB) {
        settings->boot_priority = BIOS_NVRAM_BOOT_PRIORITY_DEFAULT;
        nvram_write(BIOS_NVRAM_BOOT_PRIORITY_OFF, settings->boot_priority);
    }

    for (i = 0; i < BIOS_NVRAM_CMDLINE_MAX; ++i) {
        char ch = (char)nvram_read((unsigned char)(BIOS_NVRAM_CMDLINE_OFF + i));
        settings->linux_cmdline_suffix[i] = ch;
        if (ch == '\0') {
            terminated = 1u;
            break;
        }
    }
    if (!terminated) {
        settings->linux_cmdline_suffix[0] = '\0';
        nvram_write(BIOS_NVRAM_CMDLINE_OFF, 0u);
    }
}

void bios_nvram_save_partition(unsigned char part) {
    if (bios_nvram_enable_extended_cmos() != 0) {
        return;
    }
    nvram_write(BIOS_NVRAM_PARTITION_OFF, part);
}

void bios_nvram_save_flags0(unsigned char flags0) {
    if (bios_nvram_enable_extended_cmos() != 0) {
        return;
    }
    nvram_write(BIOS_NVRAM_FLAGS0_OFF,
                (unsigned char)(flags0 & BIOS_NVRAM_FLAGS0_KNOWN_MASK));
}

void bios_nvram_save_boot_priority(unsigned char priority) {
    if (bios_nvram_enable_extended_cmos() != 0) {
        return;
    }
    nvram_write(BIOS_NVRAM_BOOT_PRIORITY_OFF, priority);
}

void bios_nvram_save_cmdline_suffix(const char* text) {
    unsigned int i;

    if (bios_nvram_enable_extended_cmos() != 0) {
        return;
    }
    for (i = 0; i < BIOS_NVRAM_CMDLINE_MAX - 1u && text[i] != '\0'; ++i) {
        nvram_write((unsigned char)(BIOS_NVRAM_CMDLINE_OFF + i),
                    (unsigned char)text[i]);
    }
    nvram_write((unsigned char)(BIOS_NVRAM_CMDLINE_OFF + i), 0u);
    for (++i; i < BIOS_NVRAM_CMDLINE_MAX; ++i) {
        nvram_write((unsigned char)(BIOS_NVRAM_CMDLINE_OFF + i), 0u);
    }
}
