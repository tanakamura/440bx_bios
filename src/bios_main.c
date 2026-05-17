#include "bios_io.h"
#include "bios_nvram.h"
#include "bios_pci.h"
#include "bios_serial.h"
#include "bios_storage.h"
#include "blob.h"
#include "post_code.h"

void bios32_entry_c(unsigned int total_bytes, unsigned int aux_blob_linear);
__attribute__((section(".qentry"))) void bios32_qemu_entry(
    unsigned int total_bytes, unsigned int aux_blob_linear);
extern unsigned char bios16_thunk_start[];
extern unsigned char bios16_int08[];
extern unsigned char bios16_int10[];
extern unsigned char bios16_int11[];
extern unsigned char bios16_int12[];
extern unsigned char bios16_int13[];
extern unsigned char bios16_int15[];
extern unsigned char bios16_int16[];
extern unsigned char bios16_int17[];
extern unsigned char bios16_int19[];
extern unsigned char bios16_int1a[];
extern unsigned char bios16_int60[];
extern unsigned char bios16_default[];
extern unsigned char bios16_iret[];
extern void bios_boot_freedos_pm32(void);
extern void bios_call_vgabios_init_pm32(void);
extern unsigned int bios_call_vbe_mode_info_pm32(unsigned int mode);
extern unsigned int bios_call_vbe_set_mode_pm32(unsigned int mode);
extern unsigned int bios16_pm_stack_top;
extern unsigned char bios16_boot_drive[];
extern unsigned char bios16_vbe_mode_info[];
extern unsigned char bios16_thunk_end[];
extern unsigned char __bss_start[];
extern unsigned char __bss_end[];

static unsigned int bios_total_bytes_global = 0;
static unsigned int bios_dsdt_blob_linear_global = 0;
static unsigned int bios_vgabios_blob_linear_global = 0;
static unsigned int bios_test_elf_blob_linear_global = 0;
static unsigned char bios_vgabios_shadow_ready = 0;
static unsigned char bios_vgabios_initialized = 0;
static unsigned char bios_vbe_lfb_ready = 0;
static unsigned short bios_vbe_lfb_width = 0;
static unsigned short bios_vbe_lfb_height = 0;
static unsigned short bios_vbe_lfb_depth = 0;
static unsigned short bios_vbe_lfb_pitch = 0;
static unsigned int bios_vbe_lfb_base = 0;
static unsigned char bios_vbe_lfb_red_size = 0;
static unsigned char bios_vbe_lfb_red_pos = 0;
static unsigned char bios_vbe_lfb_green_size = 0;
static unsigned char bios_vbe_lfb_green_pos = 0;
static unsigned char bios_vbe_lfb_blue_size = 0;
static unsigned char bios_vbe_lfb_blue_pos = 0;
static unsigned char bios_vbe_lfb_rsvd_size = 0;
static unsigned char bios_vbe_lfb_rsvd_pos = 0;
static unsigned short bios_vbe_lfb_pages = 0;
static unsigned short bios_vbe_lfb_attrs = 0;
static unsigned char bios_qemu_mode = 0;
static unsigned char bios_maintenance_requested = 0;
static unsigned char bios_shadow_ready = 0;
static unsigned char bios_nvram_flags0 = BIOS_NVRAM_FLAGS0_DEFAULT;
static unsigned char bios_boot_priority = BIOS_NVRAM_BOOT_PRIORITY_DEFAULT;
static unsigned char bios_linux_vmlinux_partition = 0;
static unsigned char bios_enable_memtest = 0;
static unsigned char bios_run_test_blob = 0;
static char bios_linux_cmdline_suffix[BIOS_NVRAM_CMDLINE_MAX];
static const unsigned int bios16_thunk_runtime_base = 0x000fe000u;
static const unsigned int bios_runtime_gdt_linear = 0x000ff800u;
static const unsigned short bios_ebda_segment = 0x0000u;
static const unsigned short bios_dos_base_mem_kb = 640u;
static unsigned int bios_floppy_dpt_linear = 0x00000500u;
static unsigned char bios_kbd_pending_valid = 0;
static unsigned short bios_kbd_pending_ax = 0;
static unsigned int bios_tick_counter = 0;
static unsigned char bios_tick_initialized = 0;
static unsigned short bios_tick_last_raw = 0;
static unsigned int bios_tick_subcount = 0;
static unsigned char bios_timer_irq_enabled = 0;
static unsigned char bios_boot_drive = 0x80u;

#define BIOS_MTRR_SAVE_MAX 8u
struct bios_mtrr_saved_state {
    unsigned char count;
    unsigned long long def_type;
    unsigned long long base[BIOS_MTRR_SAVE_MAX];
    unsigned long long mask[BIOS_MTRR_SAVE_MAX];
};

static struct bios_mtrr_saved_state bios_memtest_mtrr_saved;

static unsigned int tsc_low(void);

static void zero_bss(void) {
    unsigned char* p = __bss_start;
    while (p < __bss_end) {
        *p++ = 0u;
    }
}

static unsigned long long rdmsr64(unsigned int msr) {
    unsigned int lo;
    unsigned int hi;
    __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((unsigned long long)hi << 32) | lo;
}

static void wrmsr64(unsigned int msr, unsigned int lo, unsigned int hi) {
    __asm__ volatile("wrmsr" : : "c"(msr), "a"(lo), "d"(hi));
}

#define IA32_MTRR_FIX4K_C0000 0x268u
#define IA32_MTRR_FIX4K_C8000 0x269u
#define IA32_MTRR_FIX4K_D0000 0x26au
#define IA32_MTRR_FIX4K_D8000 0x26bu
#define IA32_MTRR_FIX4K_E0000 0x26cu
#define IA32_MTRR_FIX4K_E8000 0x26du
#define IA32_MTRR_FIX4K_F0000 0x26eu
#define IA32_MTRR_FIX4K_F8000 0x26fu
#define IA32_MTRRCAP 0x0feu
#define IA32_MTRR_PHYSBASE0 0x200u
#define IA32_MTRR_PHYSMASK0 0x201u
#define IA32_MTRR_DEF_TYPE 0x2ffu
#define MTRR_DEF_TYPE_TYPE_MASK 0x000000ffu
#define MTRR_DEF_TYPE_E 0x00000800u
#define MTRR_PHYSMASK_VALID 0x00000800u
#define IA32_APIC_BASE 0x0000001bu
#define APIC_BASE_ENABLE 0x00000800u

#define BDA_TICK_COUNT 0x046cu
#define BDA_MIDNIGHT_FLAG 0x0470u
#define CMOS_SECONDS 0x00u
#define CMOS_MINUTES 0x02u
#define CMOS_HOURS 0x04u
#define CMOS_DAY 0x07u
#define CMOS_MONTH 0x08u
#define CMOS_YEAR 0x09u
#define CMOS_STATUS_A 0x0au
#define CMOS_STATUS_B 0x0bu
#define CMOS_STATUS_C 0x0cu
#define CMOS_STATUS_D 0x0du
#define RTC_REGA_32KHZ_RATE6 0x26u
#define RTC_REGB_SET 0x80u
#define RTC_REGB_24H_BCD 0x02u

static unsigned char cmos_read(unsigned char index) {
    outb(0x0070u, (unsigned char)(index | 0x80u));
    return inb(0x0071u);
}

static void __attribute__((unused)) rtc_dump_raw(const char* tag) {
    serial_write_string("RTC ");
    serial_write_string(tag);
    serial_write_string(" A=");
    serial_write_hex8(cmos_read(CMOS_STATUS_A));
    serial_write_string(" B=");
    serial_write_hex8(cmos_read(CMOS_STATUS_B));
    serial_write_string(" S=");
    serial_write_hex8(cmos_read(CMOS_SECONDS));
    serial_write_string(" M=");
    serial_write_hex8(cmos_read(CMOS_MINUTES));
    serial_write_string(" H=");
    serial_write_hex8(cmos_read(CMOS_HOURS));
    serial_write_string(" D=");
    serial_write_hex8(cmos_read(CMOS_DAY));
    serial_write_string(" N=");
    serial_write_hex8(cmos_read(CMOS_MONTH));
    serial_write_string(" Y=");
    serial_write_hex8(cmos_read(CMOS_YEAR));
    serial_write_string("\r\n");
}

static void cmos_write(unsigned char index, unsigned char value) {
    outb(0x0070u, (unsigned char)(index | 0x80u));
    outb(0x0071u, value);
}

#define PIIX4_ISA_DEV 7u
#define PIIX4_ISA_FN 0u
#define PIIX4_RTCCFG 0xcbu
#define PIIX4_RTCCFG_RTC_ENABLE 0x01u
#define PIIX4_RTCCFG_UPPER_RAM_EN 0x04u

static int nvram_piix4e_present(void) {
    return pci_read16(0, PIIX4_ISA_DEV, PIIX4_ISA_FN, 0x00u) == 0x8086u;
}

static int nvram_enable_extended_cmos(void) {
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

static unsigned char bin_to_bcd(unsigned char value) {
    return (unsigned char)(((value / 10u) << 4) | (value % 10u));
}

static unsigned char bcd_to_bin(unsigned char value) {
    return (unsigned char)(((value >> 4) * 10u) + (value & 0x0fu));
}

static unsigned char rtc_decode(unsigned char value, unsigned char status_b) {
    if ((status_b & 0x04u) != 0u) {
        return value;
    }
    if ((value & 0x0fu) > 9u || ((value >> 4) & 0x0fu) > 9u) {
        return 0xffu;
    }
    return bcd_to_bin(value);
}

static unsigned char rtc_decode_hour(unsigned char hour,
                                     unsigned char status_b) {
    unsigned char hour_bin =
        rtc_decode((unsigned char)(hour & 0x7fu), status_b);
    if (hour_bin == 0xffu) {
        return 0xffu;
    }
    if ((status_b & 0x02u) == 0u) {
        unsigned char pm = (unsigned char)(hour & 0x80u);
        if (hour_bin == 12u) {
            hour_bin = 0u;
        }
        if (pm != 0u && hour_bin < 12u) {
            hour_bin = (unsigned char)(hour_bin + 12u);
        }
    }
    return hour_bin;
}

static int rtc_raw_datetime_valid(unsigned char status_b, unsigned char sec,
                                  unsigned char min, unsigned char hour,
                                  unsigned char day, unsigned char mon,
                                  unsigned char year) {
    unsigned char sec_bin = rtc_decode(sec, status_b);
    unsigned char min_bin = rtc_decode(min, status_b);
    unsigned char hour_bin = rtc_decode_hour(hour, status_b);
    unsigned char day_bin = rtc_decode(day, status_b);
    unsigned char mon_bin = rtc_decode(mon, status_b);
    unsigned char year_bin = rtc_decode(year, status_b);

    return sec_bin <= 59u && min_bin <= 59u && hour_bin <= 23u &&
           day_bin >= 1u && day_bin <= 31u && mon_bin >= 1u && mon_bin <= 12u &&
           year_bin <= 99u;
}

static void rtc_write_datetime_bcd(unsigned char sec, unsigned char min,
                                   unsigned char hour, unsigned char day,
                                   unsigned char mon, unsigned char year) {
    cmos_write(CMOS_SECONDS, sec);
    cmos_write(CMOS_MINUTES, min);
    cmos_write(CMOS_HOURS, hour);
    cmos_write(CMOS_DAY, day);
    cmos_write(CMOS_MONTH, mon);
    cmos_write(CMOS_YEAR, year);
}

static void rtc_prepare_for_linux(void) {
    unsigned char status_b;
    unsigned char sec;
    unsigned char min;
    unsigned char hour;
    unsigned char day;
    unsigned char mon;
    unsigned char year;
    unsigned char valid;

    (void)nvram_enable_extended_cmos();

    status_b = cmos_read(CMOS_STATUS_B);
    cmos_write(CMOS_STATUS_B, (unsigned char)(status_b | RTC_REGB_SET));
    sec = cmos_read(CMOS_SECONDS);
    min = cmos_read(CMOS_MINUTES);
    hour = cmos_read(CMOS_HOURS);
    day = cmos_read(CMOS_DAY);
    mon = cmos_read(CMOS_MONTH);
    year = cmos_read(CMOS_YEAR);
    valid = (unsigned char)rtc_raw_datetime_valid(status_b, sec, min, hour, day,
                                                  mon, year);

    cmos_write(CMOS_STATUS_A, RTC_REGA_32KHZ_RATE6);
    if (valid != 0u) {
        rtc_write_datetime_bcd(bin_to_bcd(rtc_decode(sec, status_b)),
                               bin_to_bcd(rtc_decode(min, status_b)),
                               bin_to_bcd(rtc_decode_hour(hour, status_b)),
                               bin_to_bcd(rtc_decode(day, status_b)),
                               bin_to_bcd(rtc_decode(mon, status_b)),
                               bin_to_bcd(rtc_decode(year, status_b)));
    } else {
        rtc_write_datetime_bcd(0x00u, 0x00u, 0x00u, 0x01u, 0x01u, 0x26u);
    }
    cmos_write(CMOS_STATUS_B, RTC_REGB_24H_BCD);
    (void)cmos_read(CMOS_STATUS_C);
    (void)cmos_read(CMOS_STATUS_D);

    serial_write_string("RTC Linux sane ");
    serial_write_string(valid != 0u ? "keep\r\n" : "default\r\n");
}

static void nvram_init_defaults(void) {
    unsigned int i;

    nvram_write32(0u, BIOS_NVRAM_MAGIC);
    nvram_write(BIOS_NVRAM_PARTITION_OFF, 0u);
    nvram_write(BIOS_NVRAM_FLAGS0_OFF, BIOS_NVRAM_FLAGS0_DEFAULT);
    nvram_write(BIOS_NVRAM_BOOT_PRIORITY_OFF, BIOS_NVRAM_BOOT_PRIORITY_DEFAULT);
    for (i = BIOS_NVRAM_CMDLINE_OFF; i < BIOS_NVRAM_SIZE; ++i) {
        nvram_write((unsigned char)i, 0u);
    }
}

static void nvram_load_settings(void) {
    unsigned int i;
    unsigned char part;
    unsigned char flags0;
    unsigned char boot_priority;
    unsigned char terminated = 0;

    bios_nvram_flags0 = BIOS_NVRAM_FLAGS0_DEFAULT;
    bios_boot_priority = BIOS_NVRAM_BOOT_PRIORITY_DEFAULT;
    bios_linux_vmlinux_partition = 0u;
    bios_enable_memtest = 0u;
    bios_run_test_blob = 0u;
    bios_linux_cmdline_suffix[0] = '\0';

    if (nvram_enable_extended_cmos() != 0) {
        return;
    }
    if (nvram_read32(0u) != BIOS_NVRAM_MAGIC) {
        serial_write_string("NVRAM init\r\n");
        nvram_init_defaults();
    }

    part = nvram_read(BIOS_NVRAM_PARTITION_OFF);
    if (part > 1u) {
        part = 0u;
        nvram_write(BIOS_NVRAM_PARTITION_OFF, part);
    }
    bios_linux_vmlinux_partition = part;

    flags0 = nvram_read(BIOS_NVRAM_FLAGS0_OFF);
    if ((flags0 & ~BIOS_NVRAM_FLAGS0_KNOWN_MASK) != 0u) {
        flags0 = BIOS_NVRAM_FLAGS0_DEFAULT;
        nvram_write(BIOS_NVRAM_FLAGS0_OFF, flags0);
    }
    bios_nvram_flags0 = flags0;
    bios_enable_memtest =
        (unsigned char)((flags0 & BIOS_NVRAM_FLAGS0_MEMTEST) != 0u);
    bios_run_test_blob =
        (unsigned char)((flags0 & BIOS_NVRAM_FLAGS0_RUN_TEST_BLOB) != 0u);

    boot_priority = nvram_read(BIOS_NVRAM_BOOT_PRIORITY_OFF);
    if (boot_priority > BIOS_NVRAM_BOOT_PRIORITY_USB) {
        boot_priority = BIOS_NVRAM_BOOT_PRIORITY_DEFAULT;
        nvram_write(BIOS_NVRAM_BOOT_PRIORITY_OFF, boot_priority);
    }
    bios_boot_priority = boot_priority;

    for (i = 0; i < BIOS_NVRAM_CMDLINE_MAX; ++i) {
        char ch = (char)nvram_read((unsigned char)(BIOS_NVRAM_CMDLINE_OFF + i));
        bios_linux_cmdline_suffix[i] = ch;
        if (ch == '\0') {
            terminated = 1u;
            break;
        }
    }
    if (!terminated) {
        bios_linux_cmdline_suffix[0] = '\0';
        nvram_write(BIOS_NVRAM_CMDLINE_OFF, 0u);
    }
}

static void nvram_save_partition(unsigned char part) {
    if (nvram_enable_extended_cmos() != 0) {
        return;
    }
    nvram_write(BIOS_NVRAM_PARTITION_OFF, part);
}

static void nvram_save_flags0(unsigned char flags0) {
    if (nvram_enable_extended_cmos() != 0) {
        return;
    }
    nvram_write(BIOS_NVRAM_FLAGS0_OFF,
                (unsigned char)(flags0 & BIOS_NVRAM_FLAGS0_KNOWN_MASK));
}

static void nvram_consume_test_blob_request(void) {
    if ((bios_nvram_flags0 & BIOS_NVRAM_FLAGS0_RUN_TEST_BLOB) == 0u) {
        return;
    }
    bios_nvram_flags0 =
        (unsigned char)(bios_nvram_flags0 & ~BIOS_NVRAM_FLAGS0_RUN_TEST_BLOB);
    bios_run_test_blob = 0u;
    nvram_save_flags0(bios_nvram_flags0);
    serial_write_string("Test blob request consumed\r\n");
}

static void nvram_save_boot_priority(unsigned char priority) {
    if (nvram_enable_extended_cmos() != 0) {
        return;
    }
    nvram_write(BIOS_NVRAM_BOOT_PRIORITY_OFF, priority);
}

static void nvram_save_cmdline_suffix(const char* text) {
    unsigned int i;

    if (nvram_enable_extended_cmos() != 0) {
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

static unsigned char rtc_force_sane_mode(void) {
    unsigned char status_b = cmos_read(CMOS_STATUS_B);
    unsigned char sane_status_b = (unsigned char)((status_b & 0x79u) | 0x02u);
    if (sane_status_b != status_b) {
        cmos_write(CMOS_STATUS_B, sane_status_b);
    }
    return sane_status_b;
}

static unsigned char rtc_begin_set(void) {
    unsigned char sane_status_b = rtc_force_sane_mode();
    cmos_write(CMOS_STATUS_B, (unsigned char)(sane_status_b | 0x80u));
    return sane_status_b;
}

static void rtc_end_set(unsigned char sane_status_b) {
    cmos_write(CMOS_STATUS_B, sane_status_b);
}

static int rtc_wait_ready(void) {
    unsigned int timeout = 100000u;
    while (timeout-- != 0u) {
        if ((cmos_read(CMOS_STATUS_A) & 0x80u) == 0) {
            return 0;
        }
    }
    return -1;
}

static int rtc_read_time_bcd(unsigned char* hour_bcd, unsigned char* min_bcd,
                             unsigned char* sec_bcd) {
    unsigned char status_b;
    unsigned char hour;
    unsigned char min;
    unsigned char sec;
    unsigned char hour_bin;
    unsigned char min_bin;
    unsigned char sec_bin;

    if (rtc_wait_ready() != 0) {
        return -1;
    }
    status_b = rtc_force_sane_mode();
    sec = cmos_read(CMOS_SECONDS);
    min = cmos_read(CMOS_MINUTES);
    hour = cmos_read(CMOS_HOURS);

    if ((status_b & 0x04u) != 0) {
        sec_bin = sec;
        min_bin = min;
        hour_bin = (unsigned char)(hour & 0x7fu);
    } else {
        sec_bin = bcd_to_bin(sec);
        min_bin = bcd_to_bin(min);
        hour_bin = bcd_to_bin((unsigned char)(hour & 0x7fu));
    }

    if ((status_b & 0x02u) == 0) {
        unsigned char pm = (unsigned char)(hour & 0x80u);
        if (hour_bin == 12u) {
            hour_bin = 0u;
        }
        if (pm != 0u) {
            hour_bin = (unsigned char)(hour_bin + 12u);
        }
    }

    if (sec_bin > 59u || min_bin > 59u || hour_bin > 23u) {
        return -1;
    }

    *sec_bcd = bin_to_bcd(sec_bin);
    *min_bcd = bin_to_bcd(min_bin);
    *hour_bcd = bin_to_bcd(hour_bin);
    return 0;
}

static int rtc_read_date_bcd(unsigned char* year_bcd, unsigned char* mon_bcd,
                             unsigned char* day_bcd) {
    unsigned char status_b;
    unsigned char day;
    unsigned char mon;
    unsigned char year;
    unsigned char day_bin;
    unsigned char mon_bin;
    unsigned char year_bin;

    if (rtc_wait_ready() != 0) {
        return -1;
    }
    status_b = rtc_force_sane_mode();
    day = cmos_read(CMOS_DAY);
    mon = cmos_read(CMOS_MONTH);
    year = cmos_read(CMOS_YEAR);

    if ((status_b & 0x04u) != 0) {
        day_bin = day;
        mon_bin = mon;
        year_bin = year;
    } else {
        day_bin = bcd_to_bin(day);
        mon_bin = bcd_to_bin(mon);
        year_bin = bcd_to_bin(year);
    }

    if (day_bin < 1u || day_bin > 31u || mon_bin < 1u || mon_bin > 12u) {
        return -1;
    }

    *day_bcd = bin_to_bcd(day_bin);
    *mon_bcd = bin_to_bcd(mon_bin);
    *year_bcd = bin_to_bcd(year_bin);
    return 0;
}

static int rtc_set_time_bcd(unsigned char hour_bcd, unsigned char min_bcd,
                            unsigned char sec_bcd) {
    unsigned char sane_status_b;
    unsigned char hour_bin = bcd_to_bin((unsigned char)(hour_bcd & 0x7fu));
    unsigned char min_bin = bcd_to_bin(min_bcd);
    unsigned char sec_bin = bcd_to_bin(sec_bcd);

    if (hour_bin > 23u || min_bin > 59u || sec_bin > 59u) {
        return -1;
    }

    if (rtc_wait_ready() != 0) {
        return -1;
    }

    sane_status_b = rtc_begin_set();
    cmos_write(CMOS_SECONDS, sec_bcd);
    cmos_write(CMOS_MINUTES, min_bcd);
    cmos_write(CMOS_HOURS, (unsigned char)(hour_bcd & 0x7fu));
    rtc_end_set(sane_status_b);
    return 0;
}

static int rtc_set_date_bcd(unsigned char year_bcd, unsigned char mon_bcd,
                            unsigned char day_bcd) {
    unsigned char mon_bin = bcd_to_bin(mon_bcd);
    unsigned char day_bin = bcd_to_bin(day_bcd);
    unsigned char sane_status_b;

    if (mon_bin < 1u || mon_bin > 12u || day_bin < 1u || day_bin > 31u) {
        return -1;
    }

    if (rtc_wait_ready() != 0) {
        return -1;
    }

    sane_status_b = rtc_begin_set();
    cmos_write(CMOS_DAY, day_bcd);
    cmos_write(CMOS_MONTH, mon_bcd);
    cmos_write(CMOS_YEAR, year_bcd);
    rtc_end_set(sane_status_b);
    return 0;
}

static unsigned short pit_read_counter0(void) {
    unsigned char lo;
    unsigned char hi;
    outb(0x0043u, 0x00u);
    lo = inb(0x0040u);
    hi = inb(0x0040u);
    return (unsigned short)(((unsigned short)hi << 8) | lo);
}

static void bios_set_tick_counter(unsigned int ticks) {
    bios_tick_counter = ticks;
    *(volatile unsigned int*)BDA_TICK_COUNT = ticks;
}

static void io_wait(void) { outb(0x0080u, 0x00u); }

static void bios_disable_local_apic(void) {
    unsigned long long apic_base = rdmsr64(IA32_APIC_BASE);
    if (((unsigned int)apic_base & APIC_BASE_ENABLE) != 0u) {
        wrmsr64(IA32_APIC_BASE, ((unsigned int)apic_base & ~APIC_BASE_ENABLE),
                (unsigned int)(apic_base >> 32));
    }
}

static void bios_init_pit(void) {
    outb(0x0043u, 0x36u);
    outb(0x0040u, 0x00u);
    outb(0x0040u, 0x00u);
    bios_tick_initialized = 0;
    bios_tick_subcount = 0;
    bios_set_tick_counter(0u);
    *(volatile unsigned char*)BDA_MIDNIGHT_FLAG = 0u;
    bios_timer_irq_enabled = 0u;
}

static void bios_init_pic_for_timer(void) {
    bios_disable_local_apic();
    outb(0x0022u, 0x70u);
    io_wait();
    outb(0x0023u, 0x00u);
    io_wait();
    outb(0x0020u, 0x11u);
    io_wait();
    outb(0x00a0u, 0x11u);
    io_wait();
    outb(0x0021u, 0x08u);
    io_wait();
    outb(0x00a1u, 0x70u);
    io_wait();
    outb(0x0021u, 0x04u);
    io_wait();
    outb(0x00a1u, 0x02u);
    io_wait();
    outb(0x0021u, 0x01u);
    io_wait();
    outb(0x00a1u, 0x01u);
    io_wait();
    outb(0x0020u, 0x20u);
    outb(0x00a0u, 0x20u);
    io_wait();
    outb(0x0021u, 0xfeu);
    outb(0x00a1u, 0xffu);
    bios_timer_irq_enabled = 1u;
}

static void bios_update_tick_counter(void) {
    unsigned short raw;
    if (bios_timer_irq_enabled) {
        bios_tick_counter = *(volatile unsigned int*)BDA_TICK_COUNT;
        return;
    }

    raw = pit_read_counter0();
    if (!bios_tick_initialized) {
        bios_tick_initialized = 1;
        bios_tick_last_raw = raw;
        *(volatile unsigned int*)BDA_TICK_COUNT = bios_tick_counter;
        return;
    }

    bios_tick_subcount +=
        (unsigned short)((bios_tick_last_raw - raw) & 0xffffu);
    bios_tick_last_raw = raw;

    while (bios_tick_subcount >= 65536u) {
        bios_tick_subcount -= 65536u;
        ++bios_tick_counter;
        if (bios_tick_counter >= 0x001800b0u) {
            bios_tick_counter = 0;
            *(volatile unsigned char*)BDA_MIDNIGHT_FLAG = 1u;
        }
    }
    *(volatile unsigned int*)BDA_TICK_COUNT = bios_tick_counter;
}

#define BDA_KBD_FLAGS1 0x0417u
#define BDA_KBD_FLAGS2 0x0418u
#define BDA_KBD_HEAD 0x041au
#define BDA_KBD_TAIL 0x041cu
#define BDA_KBD_BUF_START 0x0480u
#define BDA_KBD_BUF_END 0x0482u
#define BDA_KBD_BUF_BASE 0x041eu
#define BDA_KBD_BUF_LIMIT 0x003eu
#define BDA_VIDEO_MODE 0x0449u
#define BDA_VIDEO_COLS 0x044au
#define BDA_VIDEO_PAGE_SIZE 0x044cu
#define BDA_VIDEO_PAGE_OFFSET 0x044eu
#define BDA_CURSOR_POS 0x0450u
#define BDA_CURSOR_SHAPE 0x0460u
#define BDA_ACTIVE_PAGE 0x0462u
#define BDA_VIDEO_CRTC_PORT 0x0463u
#define BDA_VIDEO_MODE_CONTROL 0x0465u
#define BDA_VIDEO_PALETTE 0x0466u
#define BDA_VIDEO_ROWS_MINUS1 0x0484u
#define BDA_VIDEO_CHAR_HEIGHT 0x0485u
#define BDA_VIDEO_CTL 0x0487u
#define BDA_VIDEO_SWITCHES 0x0488u
#define BDA_VIDEO_DCC_INDEX 0x048au

static unsigned char bios_ascii_scan_code(unsigned char ch) {
    if (ch >= '1' && ch <= '9') {
        return (unsigned char)(0x02u + (ch - '1'));
    }
    if (ch == '0') {
        return 0x0bu;
    }
    if (ch >= 'a' && ch <= 'z') {
        static const unsigned char table[26] = {
            0x1eu, 0x30u, 0x2eu, 0x20u, 0x12u, 0x21u, 0x22u, 0x23u, 0x17u,
            0x24u, 0x25u, 0x26u, 0x32u, 0x31u, 0x18u, 0x19u, 0x10u, 0x13u,
            0x1fu, 0x14u, 0x16u, 0x2fu, 0x11u, 0x2du, 0x15u, 0x2cu,
        };
        return table[ch - 'a'];
    }
    if (ch >= 'A' && ch <= 'Z') {
        return bios_ascii_scan_code((unsigned char)(ch - 'A' + 'a'));
    }
    switch (ch) {
        case '-':
        case '_':
            return 0x0cu;
        case '=':
        case '+':
            return 0x0du;
        case '[':
        case '{':
            return 0x1au;
        case ']':
        case '}':
            return 0x1bu;
        case ';':
        case ':':
            return 0x27u;
        case '\'':
        case '"':
            return 0x28u;
        case '`':
        case '~':
            return 0x29u;
        case '\\':
        case '|':
            return 0x2bu;
        case ',':
        case '<':
            return 0x33u;
        case '.':
        case '>':
            return 0x34u;
        case '/':
        case '?':
            return 0x35u;
        case ' ':
            return 0x39u;
        case '\t':
            return 0x0fu;
        default:
            return 0x00u;
    }
}

static unsigned short bios_translate_serial_key(unsigned char ch) {
    if (ch == '\r' || ch == '\n') {
        return 0x1c0du;
    }
    if (ch == 0x08u || ch == 0x7fu) {
        return 0x0e08u;
    }
    if (ch == 0x1bu) {
        return 0x011bu;
    }
    return (unsigned short)(((unsigned short)bios_ascii_scan_code(ch) << 8) |
                            ch);
}

static unsigned short* bios_cursor_slot(unsigned char page) {
    return (unsigned short*)(BDA_CURSOR_POS +
                             ((unsigned short)(page & 7u) * 2u));
}

static void bios_video_init(void) {
    unsigned int i;
    *(volatile unsigned char*)BDA_VIDEO_MODE = 0x03u;
    *(volatile unsigned short*)BDA_VIDEO_COLS = 80u;
    *(volatile unsigned short*)BDA_VIDEO_PAGE_SIZE = 0x1000u;
    *(volatile unsigned short*)BDA_VIDEO_PAGE_OFFSET = 0x0000u;
    *(volatile unsigned short*)BDA_CURSOR_SHAPE = 0x0607u;
    *(volatile unsigned char*)BDA_ACTIVE_PAGE = 0x00u;
    *(volatile unsigned short*)BDA_VIDEO_CRTC_PORT = 0x03d4u;
    *(volatile unsigned char*)BDA_VIDEO_MODE_CONTROL = 0x09u;
    *(volatile unsigned char*)BDA_VIDEO_PALETTE = 0x00u;
    *(volatile unsigned char*)BDA_VIDEO_ROWS_MINUS1 = 24u;
    *(volatile unsigned char*)BDA_VIDEO_CHAR_HEIGHT = 16u;
    *(volatile unsigned char*)BDA_VIDEO_CTL = 0x00u;
    *(volatile unsigned char*)BDA_VIDEO_SWITCHES = 0x00u;
    *(volatile unsigned char*)BDA_VIDEO_DCC_INDEX = 0x08u;
    for (i = 0; i < 8u; ++i) {
        *bios_cursor_slot((unsigned char)i) = 0x0000u;
    }
}

static unsigned short bios_get_cursor(unsigned char page) {
    return *bios_cursor_slot(page);
}

static void bios_set_cursor(unsigned char page, unsigned char row,
                            unsigned char col) {
    *bios_cursor_slot(page) =
        (unsigned short)(((unsigned short)row << 8) | col);
}

static void bios_tty_advance(unsigned char ch) {
    unsigned char page = *(volatile unsigned char*)BDA_ACTIVE_PAGE;
    unsigned short cur = bios_get_cursor(page);
    unsigned char row = (unsigned char)(cur >> 8);
    unsigned char col = (unsigned char)cur;

    if (ch == '\r') {
        col = 0;
    } else if (ch == '\n') {
        if (row < 24u) {
            ++row;
        }
    } else if (ch == '\b') {
        if (col > 0) {
            --col;
        }
    } else {
        ++col;
        if (col >= 80u) {
            col = 0;
            if (row < 24u) {
                ++row;
            }
        }
    }
    bios_set_cursor(page, row, col);
}

static void bios_kbd_init(void) {
    *(volatile unsigned char*)BDA_KBD_FLAGS1 = 0x00u;
    *(volatile unsigned char*)BDA_KBD_FLAGS2 = 0x00u;
    *(volatile unsigned short*)BDA_KBD_HEAD = 0x001eu;
    *(volatile unsigned short*)BDA_KBD_TAIL = 0x001eu;
    *(volatile unsigned short*)BDA_KBD_BUF_START = 0x001eu;
    *(volatile unsigned short*)BDA_KBD_BUF_END = 0x003eu;
}

static int bios_kbd_buf_nonempty(void) {
    return *(volatile unsigned short*)BDA_KBD_HEAD !=
           *(volatile unsigned short*)BDA_KBD_TAIL;
}

static int bios_kbd_enqueue(unsigned short ax) {
    unsigned short head = *(volatile unsigned short*)BDA_KBD_HEAD;
    unsigned short tail = *(volatile unsigned short*)BDA_KBD_TAIL;
    unsigned short next = (unsigned short)(tail + 2u);
    if (next >= BDA_KBD_BUF_LIMIT) {
        next = 0x001eu;
    }
    if (next == head) {
        return 0;
    }
    *(volatile unsigned short*)(BDA_KBD_BUF_BASE + tail - 0x001eu) = ax;
    *(volatile unsigned short*)BDA_KBD_TAIL = next;
    return 1;
}

static unsigned short bios_kbd_dequeue(void) {
    unsigned short head = *(volatile unsigned short*)BDA_KBD_HEAD;
    unsigned short value =
        *(volatile unsigned short*)(BDA_KBD_BUF_BASE + head - 0x001eu);
    head = (unsigned short)(head + 2u);
    if (head >= BDA_KBD_BUF_LIMIT) {
        head = 0x001eu;
    }
    *(volatile unsigned short*)BDA_KBD_HEAD = head;
    return value;
}

static int bios_try_fill_keybuf(void) {
    unsigned char ch;

    if (bios_kbd_buf_nonempty()) {
        return 1;
    }
    if ((inb(0x03fd) & 0x01u) == 0) {
        return 0;
    }
    ch = inb(0x03f8);
    bios_kbd_pending_ax = bios_translate_serial_key(ch);
    bios_kbd_pending_valid =
        (unsigned char)bios_kbd_enqueue(bios_kbd_pending_ax);
    if (bios_kbd_pending_valid == 0) {
        return 0;
    }
    bios_kbd_pending_valid = 0;
    return 1;
}

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

static void maintenance_print_settings(void) {
    serial_write_string("vmlinux partition=");
    serial_write_u32(bios_linux_vmlinux_partition);
    serial_write_string("\r\nflags0=");
    serial_write_hex8(bios_nvram_flags0);
    serial_write_string(" vesa=");
    serial_write_u32((bios_nvram_flags0 & BIOS_NVRAM_FLAGS0_VESA_1024_768) !=
                     0u);
    serial_write_string(" serial=");
    serial_write_u32((bios_nvram_flags0 & BIOS_NVRAM_FLAGS0_SERIAL_CONSOLE) !=
                     0u);
    serial_write_string(" memtest=");
    serial_write_u32(bios_enable_memtest);
    serial_write_string(" testblob=");
    serial_write_u32(bios_run_test_blob);
    serial_write_string("\r\nboot priority=");
    serial_write_u32(bios_boot_priority);
    serial_write_string(" (0=auto 1=ide 2=usb)\r\ncmdline='");
    serial_write_string(bios_linux_cmdline_suffix);
    serial_write_string("'\r\n");
}

static void maintenance_set_cmdline(const char* text) {
    unsigned int i;

    for (i = 0; i < BIOS_NVRAM_CMDLINE_MAX - 1u && text[i] != '\0'; ++i) {
        bios_linux_cmdline_suffix[i] = text[i];
    }
    bios_linux_cmdline_suffix[i] = '\0';
    nvram_save_cmdline_suffix(bios_linux_cmdline_suffix);
    serial_write_string("cmdline saved\r\n");
}

static void maintenance_set_partition(char ch) {
    if (ch != '0' && ch != '1') {
        serial_write_string("usage: b <0|1>\r\n");
        return;
    }
    bios_linux_vmlinux_partition = (unsigned char)(ch - '0');
    nvram_save_partition(bios_linux_vmlinux_partition);
    serial_write_string("partition saved\r\n");
}

static void maintenance_set_flag(char ch, unsigned char bit, const char* name) {
    if (ch != '0' && ch != '1') {
        serial_write_string("usage: flag <0|1>\r\n");
        return;
    }
    if (ch == '1') {
        bios_nvram_flags0 = (unsigned char)(bios_nvram_flags0 | bit);
    } else {
        bios_nvram_flags0 = (unsigned char)(bios_nvram_flags0 & ~bit);
    }
    bios_enable_memtest =
        (unsigned char)((bios_nvram_flags0 & BIOS_NVRAM_FLAGS0_MEMTEST) != 0u);
    bios_run_test_blob = (unsigned char)(
        (bios_nvram_flags0 & BIOS_NVRAM_FLAGS0_RUN_TEST_BLOB) != 0u);
    nvram_save_flags0(bios_nvram_flags0);
    serial_write_string(name);
    serial_write_string(" saved\r\n");
}

static void maintenance_set_boot_priority(char ch) {
    unsigned char priority;
    if (ch < '0' || ch > '2') {
        serial_write_string("usage: o <0|1|2>\r\n");
        return;
    }
    priority = (unsigned char)(ch - '0');
    bios_boot_priority = priority;
    nvram_save_boot_priority(priority);
    serial_write_string("boot priority saved\r\n");
}

static void maintenance_reset_defaults(void) {
    if (nvram_enable_extended_cmos() == 0) {
        nvram_init_defaults();
    }
    bios_nvram_flags0 = BIOS_NVRAM_FLAGS0_DEFAULT;
    bios_boot_priority = BIOS_NVRAM_BOOT_PRIORITY_DEFAULT;
    bios_linux_vmlinux_partition = 0u;
    bios_enable_memtest = 0u;
    bios_run_test_blob = 0u;
    bios_linux_cmdline_suffix[0] = '\0';
    serial_write_string("defaults saved\r\n");
}

static void maintenance_prompt(void) {
    char line[BIOS_NVRAM_CMDLINE_MAX + 8u];

    serial_write_string("\r\nMaintenance mode\r\n");
    serial_write_string(
        "commands: a <cmdline>, b <0|1>, m <0|1>, t <0|1>, s <0|1>, v <0|1>, o <0|1|2>, d, p, q\r\n");
    maintenance_print_settings();
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
            maintenance_print_settings();
            continue;
        }
        if (line[0] == 'd' && line[1] == '\0') {
            maintenance_reset_defaults();
            continue;
        }
        if (line[0] == 'a' && line[1] == ' ') {
            maintenance_set_cmdline(line + 2);
            continue;
        }
        if (line[0] == 'a' && line[1] == '\0') {
            maintenance_set_cmdline("");
            continue;
        }
        if (line[0] == 'b' && line[1] == ' ' && line[2] != '\0' &&
            line[3] == '\0') {
            maintenance_set_partition(line[2]);
            continue;
        }
        if (line[0] == 'm' && line[1] == ' ' && line[2] != '\0' &&
            line[3] == '\0') {
            maintenance_set_flag(line[2], BIOS_NVRAM_FLAGS0_MEMTEST,
                                 "memtest");
            continue;
        }
        if (line[0] == 't' && line[1] == ' ' && line[2] != '\0' &&
            line[3] == '\0') {
            maintenance_set_flag(line[2], BIOS_NVRAM_FLAGS0_RUN_TEST_BLOB,
                                 "test blob");
            continue;
        }
        if (line[0] == 's' && line[1] == ' ' && line[2] != '\0' &&
            line[3] == '\0') {
            maintenance_set_flag(line[2], BIOS_NVRAM_FLAGS0_SERIAL_CONSOLE,
                                 "serial console");
            continue;
        }
        if (line[0] == 'v' && line[1] == ' ' && line[2] != '\0' &&
            line[3] == '\0') {
            maintenance_set_flag(line[2], BIOS_NVRAM_FLAGS0_VESA_1024_768,
                                 "vesa");
            continue;
        }
        if (line[0] == 'o' && line[1] == ' ' && line[2] != '\0' &&
            line[3] == '\0') {
            maintenance_set_boot_priority(line[2]);
            continue;
        }
        serial_write_string("?\r\n");
    }
}

static const unsigned long long bios_gdt_template[] = {
    0x0000000000000000ull, 0x00cf9b000000ffffull, 0x00cf93000000ffffull,
    0x00009b0fe000ffffull, 0x0000930fe000ffffull,
};

struct gdtr32 {
    unsigned short limit;
    unsigned int base;
} __attribute__((packed));

static void load_bios_gdt(const unsigned long long* gdt) {
    struct gdtr32 gdtr;
    gdtr.limit = (unsigned short)(sizeof(bios_gdt_template) - 1u);
    gdtr.base = (unsigned int)gdt;

    __asm__ volatile("lgdt %0" : : "m"(gdtr) : "memory");
    __asm__ volatile(
        "movw $0x10, %%ax\n\t"
        "movw %%ax, %%ds\n\t"
        "movw %%ax, %%es\n\t"
        "movw %%ax, %%fs\n\t"
        "movw %%ax, %%gs\n\t"
        "movw %%ax, %%ss\n\t"
        :
        :
        : "eax", "memory");
}

static void cache_writeback_invalidate(void) {
    __asm__ volatile("wbinvd" : : : "memory");
}

static void cpu_serialize(void) {
    __asm__ volatile("xorl %%eax, %%eax\n\tcpuid"
                     :
                     :
                     : "eax", "ebx", "ecx", "edx", "memory");
}

static void cache_disable_for_mtrr_update(void) {
    __asm__ volatile(
        "movl %%cr0, %%eax\n\t"
        "orl $0x40000000, %%eax\n\t"
        "andl $0xdfffffff, %%eax\n\t"
        "movl %%eax, %%cr0\n\t"
        "wbinvd"
        :
        :
        : "eax", "memory");
}

static void cache_enable_after_mtrr_update(void) {
    __asm__ volatile(
        "wbinvd\n\t"
        "movl %%cr0, %%eax\n\t"
        "andl $0x9fffffff, %%eax\n\t"
        "movl %%eax, %%cr0"
        :
        :
        : "eax", "memory");
}

static void serialize_instruction_stream(void) {
    __asm__ volatile("xorl %%eax, %%eax\n\tcpuid"
                     :
                     :
                     : "eax", "ebx", "ecx", "edx", "memory");
}

static void enable_shadow_wb_mtrrs(void) {
    unsigned long long def_type = rdmsr64(IA32_MTRR_DEF_TYPE);
    unsigned int def_lo = (unsigned int)def_type;
    unsigned int def_hi = (unsigned int)(def_type >> 32);

    cache_disable_for_mtrr_update();
    wrmsr64(IA32_MTRR_DEF_TYPE, (def_lo & ~MTRR_DEF_TYPE_E), def_hi);
    wrmsr64(IA32_MTRR_FIX4K_C0000, 0x06060606u, 0x06060606u);
    wrmsr64(IA32_MTRR_FIX4K_C8000, 0x06060606u, 0x06060606u);
    wrmsr64(IA32_MTRR_FIX4K_D0000, 0x06060606u, 0x06060606u);
    wrmsr64(IA32_MTRR_FIX4K_D8000, 0x06060606u, 0x06060606u);
    wrmsr64(IA32_MTRR_FIX4K_E0000, 0x06060606u, 0x06060606u);
    wrmsr64(IA32_MTRR_FIX4K_E8000, 0x06060606u, 0x06060606u);
    wrmsr64(IA32_MTRR_FIX4K_F0000, 0x06060606u, 0x06060606u);
    wrmsr64(IA32_MTRR_FIX4K_F8000, 0x06060606u, 0x06060606u);
    wrmsr64(IA32_MTRR_DEF_TYPE, def_lo, def_hi);
    cache_enable_after_mtrr_update();
    serial_write_string("MTRR shadow C-F WB\r\n");
}

static unsigned char mtrr_variable_count(void) {
    unsigned int count = (unsigned int)(rdmsr64(IA32_MTRRCAP) & 0xffu);
    if (count > BIOS_MTRR_SAVE_MAX) {
        count = BIOS_MTRR_SAVE_MAX;
    }
    return (unsigned char)count;
}

static void mtrr_save_and_uc_1m_plus(struct bios_mtrr_saved_state* saved) {
    unsigned int i;
    unsigned int def_lo;
    unsigned int def_hi;

    saved->count = mtrr_variable_count();
    saved->def_type = rdmsr64(IA32_MTRR_DEF_TYPE);
    for (i = 0u; i < saved->count; ++i) {
        saved->base[i] = rdmsr64(IA32_MTRR_PHYSBASE0 + i * 2u);
        saved->mask[i] = rdmsr64(IA32_MTRR_PHYSMASK0 + i * 2u);
    }

    def_lo = (unsigned int)saved->def_type;
    def_hi = (unsigned int)(saved->def_type >> 32);

    cache_disable_for_mtrr_update();
    wrmsr64(IA32_MTRR_DEF_TYPE, def_lo & ~MTRR_DEF_TYPE_E, def_hi);
    for (i = 0u; i < saved->count; ++i) {
        unsigned int mask_lo =
            (unsigned int)saved->mask[i] & ~MTRR_PHYSMASK_VALID;
        unsigned int mask_hi = (unsigned int)(saved->mask[i] >> 32);
        wrmsr64(IA32_MTRR_PHYSMASK0 + i * 2u, mask_lo, mask_hi);
    }
    wrmsr64(IA32_MTRR_DEF_TYPE, def_lo & ~MTRR_DEF_TYPE_TYPE_MASK, def_hi);
    cache_enable_after_mtrr_update();
}

static void mtrr_restore_saved(const struct bios_mtrr_saved_state* saved) {
    unsigned int i;
    unsigned int def_lo = (unsigned int)saved->def_type;
    unsigned int def_hi = (unsigned int)(saved->def_type >> 32);

    cache_disable_for_mtrr_update();
    wrmsr64(IA32_MTRR_DEF_TYPE, def_lo & ~MTRR_DEF_TYPE_E, def_hi);
    for (i = 0u; i < saved->count; ++i) {
        wrmsr64(IA32_MTRR_PHYSBASE0 + i * 2u,
                (unsigned int)saved->base[i],
                (unsigned int)(saved->base[i] >> 32));
        wrmsr64(IA32_MTRR_PHYSMASK0 + i * 2u,
                (unsigned int)saved->mask[i],
                (unsigned int)(saved->mask[i] >> 32));
    }
    wrmsr64(IA32_MTRR_DEF_TYPE, def_lo, def_hi);
    cache_enable_after_mtrr_update();
}

static void enable_shadow_dram(void) {
    unsigned char old_pam0;
    unsigned char pam;

    old_pam0 = pci_read8(0, 0, 0, 0x59);

    cache_writeback_invalidate();
    pci_write8(0, 0, 0, 0x59, (unsigned char)(old_pam0 | 0x30u));
    for (pam = 0x5au; pam <= 0x5fu; ++pam) {
        pci_write8(0, 0, 0, pam, 0x33u);
    }
    cache_writeback_invalidate();

    serial_write_string("PAM shadow RAM C-F old=");
    serial_write_hex8(old_pam0);
    serial_write_string(" new=");
    serial_write_hex8(pci_read8(0, 0, 0, 0x59));
    serial_write_string("\r\n");
}

static void clear_shadow_window(void) {
    volatile unsigned int* p = (volatile unsigned int*)0x000c0000u;
    volatile unsigned int* end = (volatile unsigned int*)0x000e0000u;

    while (p < end) {
        *p++ = 0u;
    }
}

static void install_runtime_gdt(void) {
    volatile unsigned long long* gdt =
        (volatile unsigned long long*)bios_runtime_gdt_linear;
    unsigned int i;

    for (i = 0; i < sizeof(bios_gdt_template) / sizeof(bios_gdt_template[0]);
         ++i) {
        gdt[i] = bios_gdt_template[i];
    }
    load_bios_gdt((const unsigned long long*)bios_runtime_gdt_linear);
}

#define VGA_BIOS_LINEAR 0x000c0000u
#define VGA_BIOS_CAPACITY (BIOS_LOAD_LINEAR - VGA_BIOS_LINEAR)

static void install_bios_shadow(void) {
    if (bios_shadow_ready != 0u) {
        install_runtime_gdt();
        serial_write_string("PAM shadow already ready\r\n");
        return;
    }

    load_bios_gdt(bios_gdt_template);
    enable_shadow_dram();
    clear_shadow_window();
    enable_shadow_wb_mtrrs();
    install_runtime_gdt();
}

static void install_vgabios_shadow(void) {
    blob_expand_fn expand = (blob_expand_fn)BLOB_SERVICE_LINEAR;
    struct blob_status status;
    unsigned int size;
    unsigned int i;
    unsigned char sum = 0u;
    int rc;

    bios_vgabios_shadow_ready = 0u;
    if (bios_vgabios_blob_linear_global == 0u) {
        return;
    }

    serial_write_string("VBIOS @ 000c0000...");
    rc = expand((const void*)bios_vgabios_blob_linear_global,
                (void*)BLOB_STAGE_LINEAR, (void*)VGA_BIOS_LINEAR,
                VGA_BIOS_CAPACITY, &status);
    serial_write_string("\r\n");
    if (rc != 0) {
        serial_write_string("VBIOS blob failed rc=");
        serial_write_hex8((unsigned char)rc);
        serial_write_string(" block=");
        serial_write_hex32(status.block);
        serial_write_string("\r\n");
        return;
    }

    if (*(volatile unsigned char*)VGA_BIOS_LINEAR != 0x55u ||
        *(volatile unsigned char*)(VGA_BIOS_LINEAR + 1u) != 0xaau) {
        serial_write_string("VBIOS bad signature\r\n");
        return;
    }

    size =
        (unsigned int)(*(volatile unsigned char*)(VGA_BIOS_LINEAR + 2u)) * 512u;
    if (size == 0u || size > VGA_BIOS_CAPACITY) {
        serial_write_string("VBIOS bad size\r\n");
        return;
    }
    for (i = 0u; i < size; ++i) {
        sum = (unsigned char)(sum +
                              *(volatile unsigned char*)(VGA_BIOS_LINEAR + i));
    }
    if (sum != 0u) {
        serial_write_string("VBIOS bad checksum=");
        serial_write_hex8(sum);
        serial_write_string("\r\n");
        return;
    }

    bios_vgabios_shadow_ready = 1u;
    serial_write_string("VBIOS ok size=");
    serial_write_hex8(*(volatile unsigned char*)(VGA_BIOS_LINEAR + 2u));
    serial_write_string("*512\r\n");
}

static void init_vgabios_for_linux(void) {
    if (bios_vgabios_shadow_ready == 0u || bios_vgabios_initialized != 0u) {
        return;
    }
    serial_write_string("VBIOS init C000:0003...\r\n");
    cache_writeback_invalidate();
    bios_call_vgabios_init_pm32();
    cache_writeback_invalidate();
    bios_vgabios_initialized = 1u;
    serial_write_string("VBIOS init returned\r\n");
}

static void install_ivt_vector(unsigned char vector, unsigned int linear) {
    volatile unsigned short* ivt = (volatile unsigned short*)0x00000000u;
    unsigned short offset;
    unsigned short segment;

    if (linear >= 0x000ffff0u && linear <= 0x0010ffefu) {
        segment = 0xffffu;
        offset = (unsigned short)(linear - 0x000ffff0u);
    } else {
        segment = (unsigned short)(linear >> 4);
        offset = (unsigned short)(linear & 0x000fu);
    }

    ivt[(unsigned int)vector * 2u + 0u] = offset;
    ivt[(unsigned int)vector * 2u + 1u] = segment;
}

static void install_thunk_vector(unsigned char vector, unsigned int linear) {
    volatile unsigned short* ivt = (volatile unsigned short*)0x00000000u;
    unsigned short segment = (unsigned short)(bios16_thunk_runtime_base >> 4);
    unsigned short offset =
        (unsigned short)(linear - bios16_thunk_runtime_base);

    ivt[(unsigned int)vector * 2u + 0u] = offset;
    ivt[(unsigned int)vector * 2u + 1u] = segment;
}

static void install_bios_thunks(void) {
    static const unsigned char floppy_dpt[11] = {
        0xaf, 0x02, 0x25, 0x02, 0x12, 0x1b, 0xff, 0x6c, 0xf6, 0x0f, 0x08,
    };
    volatile unsigned char* thunk =
        (volatile unsigned char*)bios16_thunk_runtime_base;
    unsigned int thunk_size =
        (unsigned int)(bios16_thunk_end - bios16_thunk_start);
    unsigned int dpt_linear =
        bios16_thunk_runtime_base + ((thunk_size + 15u) & ~15u);
    volatile unsigned char* dpt = (volatile unsigned char*)dpt_linear;
    unsigned int thunk_off;
    unsigned int i;
    unsigned int default_linear =
        bios16_thunk_runtime_base +
        (unsigned int)(bios16_default - bios16_thunk_start);

    install_bios_shadow();

    for (thunk_off = 0; thunk_off < thunk_size; ++thunk_off) {
        thunk[thunk_off] = bios16_thunk_start[thunk_off];
    }

    for (i = 0; i < 256u; ++i) {
        install_ivt_vector((unsigned char)i, default_linear);
    }

    install_thunk_vector(0x10,
                         bios16_thunk_runtime_base +
                             (unsigned int)(bios16_int10 - bios16_thunk_start));
    install_thunk_vector(0x08,
                         bios16_thunk_runtime_base +
                             (unsigned int)(bios16_int08 - bios16_thunk_start));
    install_thunk_vector(0x11,
                         bios16_thunk_runtime_base +
                             (unsigned int)(bios16_int11 - bios16_thunk_start));
    install_thunk_vector(0x12,
                         bios16_thunk_runtime_base +
                             (unsigned int)(bios16_int12 - bios16_thunk_start));
    install_thunk_vector(0x13,
                         bios16_thunk_runtime_base +
                             (unsigned int)(bios16_int13 - bios16_thunk_start));
    install_thunk_vector(0x15,
                         bios16_thunk_runtime_base +
                             (unsigned int)(bios16_int15 - bios16_thunk_start));
    install_thunk_vector(0x16,
                         bios16_thunk_runtime_base +
                             (unsigned int)(bios16_int16 - bios16_thunk_start));
    install_thunk_vector(0x17,
                         bios16_thunk_runtime_base +
                             (unsigned int)(bios16_int17 - bios16_thunk_start));
    install_thunk_vector(0x19,
                         bios16_thunk_runtime_base +
                             (unsigned int)(bios16_int19 - bios16_thunk_start));
    install_thunk_vector(0x1a,
                         bios16_thunk_runtime_base +
                             (unsigned int)(bios16_int1a - bios16_thunk_start));
    install_thunk_vector(0x60,
                         bios16_thunk_runtime_base +
                             (unsigned int)(bios16_int60 - bios16_thunk_start));
    install_thunk_vector(0x1c,
                         bios16_thunk_runtime_base +
                             (unsigned int)(bios16_iret - bios16_thunk_start));
    bios_floppy_dpt_linear = dpt_linear;
    install_ivt_vector(0x1e, dpt_linear);
    install_thunk_vector(0x40,
                         bios16_thunk_runtime_base +
                             (unsigned int)(bios16_int13 - bios16_thunk_start));
    for (i = 0; i < sizeof(floppy_dpt); ++i) {
        dpt[i] = floppy_dpt[i];
    }
    serialize_instruction_stream();
    *(volatile unsigned short*)0x0410u = 0x0000u;
    *(volatile unsigned short*)0x0413u = bios_dos_base_mem_kb;
    *(volatile unsigned short*)0x040eu = bios_ebda_segment;
    bios_kbd_init();
    bios_video_init();
    *(volatile unsigned char*)0x043eu = 0x01u;
    *(volatile unsigned char*)0x043fu = 0x00u;
    *(volatile unsigned char*)0x0440u = 0x25u;
    *(volatile unsigned char*)0x0441u = 0x00u;
    *(volatile unsigned char*)0x0474u = 0x00u;
    *(volatile unsigned char*)0x0475u = bios_hdd_is_present() ? 1u : 0u;
    *(volatile unsigned char*)0x048bu = 0x00u;
    *(volatile unsigned char*)0x048cu = 0x00u;
    *(volatile unsigned char*)0x048du = 0x00u;
    *(volatile unsigned char*)0x048eu = 0x00u;
    *(volatile unsigned char*)0x048fu = 0x07u;
    *(volatile unsigned char*)0x0490u = 0x17u;
    *(volatile unsigned char*)0x0491u = 0x00u;
    *(volatile unsigned char*)0x0492u = 0x00u;
    bios_init_pit();
    bios_init_pic_for_timer();
}

struct rm_int13_frame {
    unsigned short ax;
    unsigned short bx;
    unsigned short cx;
    unsigned short dx;
    unsigned short si;
    unsigned short di;
    unsigned short es;
    unsigned short ds;
    unsigned short bp;
    unsigned short ip;
    unsigned short cs;
    unsigned short flags;
    unsigned int eax32;
    unsigned int ebx32;
    unsigned int ecx32;
    unsigned int edx32;
    unsigned int esi32;
    unsigned int edi32;
    unsigned int ebp32;
    unsigned short fs;
    unsigned short gs;
} __attribute__((packed));

struct e820_entry {
    unsigned int base_low;
    unsigned int base_high;
    unsigned int length_low;
    unsigned int length_high;
    unsigned int type;
} __attribute__((packed));

struct rm_dap {
    unsigned char size;
    unsigned char reserved;
    unsigned short count;
    unsigned short off;
    unsigned short seg;
    unsigned int lba_low;
    unsigned int lba_high;
} __attribute__((packed));

struct rm_edd_params {
    unsigned short size;
    unsigned short information;
    unsigned int cylinders;
    unsigned int heads;
    unsigned int sectors;
    unsigned int total_sectors_low;
    unsigned int total_sectors_high;
    unsigned short bytes_per_sector;
    unsigned int edd_config_params;
} __attribute__((packed));

#define BOOT_SECTOR_LINEAR 0x00007c00u
#define BIOS_TOP_RESERVED_SIZE 0x00100000u
#define BIOS_MEMTEST_START 0x00100000u
#define BIOS_MEMTEST_MARK_STEP 0x00100000u
#define BLOB_SERVICE_RESERVED_SIZE 0x00001000u
#define E820_TYPE_USABLE 1u
#define E820_TYPE_RESERVED 2u
#define E820_SMAP 0x534d4150u

static void nvram_record_boot_success(unsigned char kind);

static unsigned int rm_seg_off_to_linear(unsigned short seg,
                                         unsigned short off) {
    return ((unsigned int)seg << 4) + off;
}

static void rm_set_cf(struct rm_int13_frame* f) { f->flags |= 0x0001u; }

static void rm_clear_cf(struct rm_int13_frame* f) { f->flags &= 0xfffeu; }

static int prepare_boot_sector_current(void) {
    if (bios_hdd_load_mbr_boot_sector(BOOT_SECTOR_LINEAR) == 0) {
        bios_boot_drive = 0x80u;
        nvram_record_boot_success(bios_hdd_current_kind());
        return 0;
    }
    return -1;
}

static void prepare_boot_sector(void) {
    if (bios_boot_priority == BIOS_NVRAM_BOOT_PRIORITY_IDE) {
        if (bios_hdd_select_kind(BIOS_HDD_KIND_IDE) != 0u &&
            prepare_boot_sector_current() == 0) {
            return;
        }
    } else if (bios_boot_priority == BIOS_NVRAM_BOOT_PRIORITY_USB) {
        if (bios_hdd_select_kind(BIOS_HDD_KIND_USB) != 0u &&
            prepare_boot_sector_current() == 0) {
            return;
        }
    } else {
        if (bios_hdd_select_kind(BIOS_HDD_KIND_IDE) != 0u &&
            prepare_boot_sector_current() == 0) {
            return;
        }
        if (bios_hdd_select_kind(BIOS_HDD_KIND_USB) != 0u &&
            prepare_boot_sector_current() == 0) {
            return;
        }
    }
    serial_write_string("No bootable HDD MBR\r\n");
    for (;;) {
        __asm__ volatile("hlt");
    }
}

static void install_boot_drive(void) {
    *(volatile unsigned char*)(bios16_thunk_runtime_base +
                               (unsigned int)(bios16_boot_drive -
                                              bios16_thunk_start)) =
        bios_boot_drive;
}

static void rm_set_zf(struct rm_int13_frame* f) { f->flags |= 0x0040u; }

static void rm_clear_zf(struct rm_int13_frame* f) { f->flags &= ~0x0040u; }

static void rm_return_eax32(struct rm_int13_frame* f, unsigned int value) {
    f->eax32 = value;
    f->ax = (unsigned short)value;
}

static void rm_return_ebx32(struct rm_int13_frame* f, unsigned int value) {
    f->ebx32 = value;
    f->bx = (unsigned short)value;
}

static void rm_return_ecx32(struct rm_int13_frame* f, unsigned int value) {
    f->ecx32 = value;
    f->cx = (unsigned short)value;
}

static void rm_return_edx32(struct rm_int13_frame* f, unsigned int value) {
    f->edx32 = value;
    f->dx = (unsigned short)value;
}

static void bios_memset(void* dst, unsigned char value, unsigned int len) {
    unsigned char* p = (unsigned char*)dst;
    while (len-- != 0u) {
        *p++ = value;
    }
}

static void bios_memcpy(void* dst, const void* src, unsigned int len) {
    unsigned char* d = (unsigned char*)dst;
    const unsigned char* s = (const unsigned char*)src;
    while (len-- != 0u) {
        *d++ = *s++;
    }
}

static unsigned int bios_top_reserved_base(void) {
    if (bios_total_bytes_global <= 0x00100000u) {
        return bios_total_bytes_global;
    }
    if (bios_total_bytes_global <= 0x00200000u) {
        return 0x00100000u;
    }
    return (bios_total_bytes_global - BIOS_TOP_RESERVED_SIZE) & ~0xfffu;
}

static unsigned int bios_pm_stack_top(void) {
    if (bios_total_bytes_global >= 0x00300000u) {
        return (bios_total_bytes_global & ~0xfffu) - 0x1000u;
    }
    return 0x001ff000u;
}

static void install_pm_stack_top(void) {
    *(volatile unsigned int*)(bios16_thunk_runtime_base +
                              (unsigned int)((unsigned char*)&bios16_pm_stack_top -
                                             bios16_thunk_start)) =
        bios_pm_stack_top();
}

static unsigned int bios_extended_usable_end(void) {
    unsigned int top_reserved = bios_top_reserved_base();
    if (top_reserved > bios_total_bytes_global) {
        return bios_total_bytes_global;
    }
    return top_reserved;
}

static void bios_memtest_print_kib_ok(unsigned int bytes) {
    unsigned int kib = bytes >> 10;
    unsigned int divisor = 1000000u;

    while (divisor != 0u) {
        serial_write_char((char)('0' + ((kib / divisor) % 10u)));
        divisor /= 10u;
    }
    serial_write_string(" KiB OK");
}

static void bios_memtest_progress(unsigned int addr, unsigned int* next_mark) {
    while (addr >= *next_mark) {
        serial_write_char('\r');
        bios_memtest_print_kib_ok(*next_mark);
        *next_mark += BIOS_MEMTEST_MARK_STEP;
    }
}

static unsigned int bios_memtest_skip_end(unsigned int addr) {
    if (addr >= BLOB_SERVICE_LINEAR &&
        addr < BLOB_SERVICE_LINEAR + BLOB_SERVICE_RESERVED_SIZE) {
        return BLOB_SERVICE_LINEAR + BLOB_SERVICE_RESERVED_SIZE;
    }
    if (addr >= BLOB_STAGE_LINEAR &&
        addr < BLOB_STAGE_LINEAR + BLOB_STAGE_CAPACITY) {
        return BLOB_STAGE_LINEAR + BLOB_STAGE_CAPACITY;
    }
    return addr;
}

static int bios_memtest_range_uncached(unsigned int end) {
    unsigned int addr;
    unsigned int next_mark = BIOS_MEMTEST_START + BIOS_MEMTEST_MARK_STEP;

    if (end <= BIOS_MEMTEST_START) {
        return 0;
    }

    bios_memtest_print_kib_ok(BIOS_MEMTEST_START);
    for (addr = BIOS_MEMTEST_START; addr + 4u <= end;) {
        unsigned int skip_end = bios_memtest_skip_end(addr);
        if (skip_end != addr) {
            addr = skip_end;
            bios_memtest_progress(addr, &next_mark);
            continue;
        }
        *(volatile unsigned int*)addr = addr ^ 0xa5a55a5au;
        addr += 4u;
        bios_memtest_progress(addr, &next_mark);
    }
    for (addr = BIOS_MEMTEST_START; addr + 4u <= end;) {
        unsigned int expected;
        unsigned int got;
        unsigned int skip_end = bios_memtest_skip_end(addr);
        if (skip_end != addr) {
            addr = skip_end;
            continue;
        }
        expected = addr ^ 0xa5a55a5au;
        got = *(volatile unsigned int*)addr;
        if (got != expected) {
            serial_write_string("\r\nMemTest fail @ ");
            serial_write_hex32(addr);
            serial_write_string(" got=");
            serial_write_hex32(got);
            serial_write_string(" exp=");
            serial_write_hex32(expected);
            serial_write_string("\r\n");
            return -1;
        }
        *(volatile unsigned int*)addr = ~expected;
        addr += 4u;
    }
    serial_write_string("\r\n");
    return 0;
}

static void bios_run_optional_memtest(void) {
    unsigned int end;
    int rc;

    if (bios_enable_memtest == 0u) {
        return;
    }

    end = bios_extended_usable_end();
    serial_write_string("Memtest UC ");
    serial_write_hex32(BIOS_MEMTEST_START);
    serial_write_string("-");
    serial_write_hex32(end);
    serial_write_string("\r\n");

    mtrr_save_and_uc_1m_plus(&bios_memtest_mtrr_saved);
    rc = bios_memtest_range_uncached(end);
    mtrr_restore_saved(&bios_memtest_mtrr_saved);

    if (rc != 0) {
        outb(0x80, POST_DRAM_TEST_FAIL);
        for (;;) {
            __asm__ volatile("hlt");
        }
    }
    serial_write_string("Memtest ok\r\n");
}

static unsigned int e820_entry_count(void) {
    if (bios_total_bytes_global <= 0x00100000u) {
        return 2u;
    }
    if (bios_top_reserved_base() > 0x00100000u) {
        return 4u;
    }
    return 3u;
}

static void e820_set_entry(struct e820_entry* entry, unsigned int base,
                           unsigned int length, unsigned int type) {
    entry->base_low = base;
    entry->base_high = 0u;
    entry->length_low = length;
    entry->length_high = 0u;
    entry->type = type;
}

static int e820_get_entry(unsigned int index, struct e820_entry* entry) {
    unsigned int usable_end = bios_extended_usable_end();
    unsigned int top_reserved = bios_top_reserved_base();

    switch (index) {
        case 0:
            e820_set_entry(entry, 0x00000000u, 0x0009fc00u, E820_TYPE_USABLE);
            return 0;
        case 1:
            e820_set_entry(entry, 0x0009fc00u, 0x00060400u, E820_TYPE_RESERVED);
            return 0;
        case 2:
            if (bios_total_bytes_global <= 0x00100000u) {
                return -1;
            }
            if (usable_end <= 0x00100000u) {
                e820_set_entry(entry, 0x00100000u,
                               bios_total_bytes_global - 0x00100000u,
                               E820_TYPE_RESERVED);
            } else {
                e820_set_entry(entry, 0x00100000u, usable_end - 0x00100000u,
                               E820_TYPE_USABLE);
            }
            return 0;
        case 3:
            if (top_reserved <= 0x00100000u ||
                bios_total_bytes_global <= top_reserved) {
                return -1;
            }
            e820_set_entry(entry, top_reserved,
                           bios_total_bytes_global - top_reserved,
                           E820_TYPE_RESERVED);
            return 0;
        default:
            return -1;
    }
}

#define ACPI_RSDP_LINEAR 0x0009fc00u
#define ACPI_EBDA_SEGMENT 0x9fc0u
#define ACPI_TABLE_RESERVED_OFFSET 0x00080000u
#define ACPI_LOW_TABLE_LINEAR 0x000d0000u
#define ACPI_LOW_TABLE_CAPACITY 0x00010000u
#define FW_CFG_PORT_SEL 0x0510u
#define FW_CFG_PORT_DATA 0x0511u
#define FW_CFG_SIGNATURE 0x0000u
#define FW_CFG_FILE_DIR 0x0019u
#define FW_CFG_MAX_FILE_PATH 56u
#define QEMU_ACPI_PM_BASE 0x0000b000u
#define ACPI_REAL_PM1_EVT 0x0000e400u
#define ACPI_REAL_PM1_CNT 0x0000e404u
#define ACPI_REAL_GPE0 0x0000e40cu
#define ACPI_GPE0_LEN 4u
#define ACPI_REAL_PCI_DEV 7u
#define ACPI_REAL_PCI_FN 3u
#define ACPI_PM1_CNT_SCI_EN 0x0001u

struct acpi_table_ref {
    unsigned int sig;
    unsigned int addr;
    unsigned int len;
};

static unsigned int acpi_sig(const char* s) {
    return (unsigned int)(unsigned char)s[0] |
           ((unsigned int)(unsigned char)s[1] << 8) |
           ((unsigned int)(unsigned char)s[2] << 16) |
           ((unsigned int)(unsigned char)s[3] << 24);
}

static unsigned int acpi_get32(const unsigned char* p) {
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8) |
           ((unsigned int)p[2] << 16) | ((unsigned int)p[3] << 24);
}

static void acpi_put16(unsigned char* p, unsigned short value) {
    p[0] = (unsigned char)value;
    p[1] = (unsigned char)(value >> 8);
}

static void acpi_put32(unsigned char* p, unsigned int value) {
    p[0] = (unsigned char)value;
    p[1] = (unsigned char)(value >> 8);
    p[2] = (unsigned char)(value >> 16);
    p[3] = (unsigned char)(value >> 24);
}

static void acpi_put64(unsigned char* p, unsigned int value) {
    acpi_put32(p, value);
    acpi_put32(p + 4, 0u);
}

static void acpi_write_bytes(unsigned char* dst, const char* src,
                             unsigned int len) {
    unsigned int i;
    for (i = 0; i < len; ++i) {
        dst[i] = (unsigned char)src[i];
    }
}

static int acpi_name_eq(const char* a, const char* b) {
    while (*a != '\0' || *b != '\0') {
        if (*a != *b) {
            return 0;
        }
        ++a;
        ++b;
    }
    return 1;
}

static unsigned char acpi_checksum(const unsigned char* p, unsigned int len) {
    unsigned int sum = 0u;
    unsigned int i;
    for (i = 0; i < len; ++i) {
        sum += p[i];
    }
    return (unsigned char)(0u - sum);
}

static void acpi_fix_table_checksum(unsigned char* table) {
    unsigned int len = acpi_get32(table + 4);
    table[9] = 0u;
    table[9] = acpi_checksum(table, len);
}

static unsigned int acpi_table_base(void) {
    unsigned int top = bios_top_reserved_base();
    if (top >= 0x00100000u &&
        bios_total_bytes_global >= top + BIOS_TOP_RESERVED_SIZE) {
        return top + ACPI_TABLE_RESERVED_OFFSET;
    }
    return ACPI_LOW_TABLE_LINEAR;
}

static unsigned int acpi_table_capacity(void) {
    unsigned int top = bios_top_reserved_base();
    if (top >= 0x00100000u &&
        bios_total_bytes_global >= top + BIOS_TOP_RESERVED_SIZE) {
        return BIOS_TOP_RESERVED_SIZE - ACPI_TABLE_RESERVED_OFFSET;
    }
    return ACPI_LOW_TABLE_CAPACITY;
}

static void acpi_install_rsdp(const char* oem_id, unsigned int rsdt_addr) {
    unsigned char* rsdp = (unsigned char*)ACPI_RSDP_LINEAR;
    volatile unsigned short* ebda = (volatile unsigned short*)0x0000040eu;

    bios_memset(rsdp, 0u, 20u);
    acpi_write_bytes(rsdp + 0, "RSD PTR ", 8u);
    acpi_write_bytes(rsdp + 9, oem_id, 6u);
    rsdp[15] = 0u;
    acpi_put32(rsdp + 16, rsdt_addr);
    rsdp[8] = acpi_checksum(rsdp, 20u);
    *ebda = ACPI_EBDA_SEGMENT;
}

static void acpi_build_header(unsigned char* table, const char* sig,
                              unsigned int len, unsigned char rev,
                              const char* oem_id, const char* table_id) {
    bios_memset(table, 0u, len);
    acpi_write_bytes(table + 0, sig, 4u);
    acpi_put32(table + 4, len);
    table[8] = rev;
    acpi_write_bytes(table + 10, oem_id, 6u);
    acpi_write_bytes(table + 16, table_id, 8u);
    acpi_put32(table + 24, 0x00000001u);
    acpi_write_bytes(table + 28, "440B", 4u);
    acpi_put32(table + 32, 0x00000001u);
}

static void acpi_build_real_fadt(unsigned char* fadt, unsigned int facs,
                                 unsigned int dsdt) {
    acpi_build_header(fadt, "FACP", 0x74u, 1u, "ASUS  ", "P2B98-XV");
    acpi_put32(fadt + 24, 0x58582e31u);
    acpi_write_bytes(fadt + 28, "ASUS", 4u);
    acpi_put32(fadt + 32, 0x31303030u);
    acpi_put32(fadt + 36, facs);
    acpi_put32(fadt + 40, dsdt);
    acpi_put16(fadt + 46, 9u);
    acpi_put32(fadt + 48, 0u);
    fadt[52] = 0u;
    fadt[53] = 0u;
    acpi_put32(fadt + 56, 0x0000e400u);
    acpi_put32(fadt + 64, 0x0000e404u);
    acpi_put32(fadt + 76, 0x0000e408u);
    acpi_put32(fadt + 80, 0x0000e40cu);
    fadt[88] = 4u;
    fadt[89] = 2u;
    fadt[91] = 4u;
    fadt[92] = 4u;
    acpi_put16(fadt + 96, 0x005au);
    acpi_put16(fadt + 98, 0x0384u);
    fadt[104] = 1u;
    fadt[106] = 0x0du;
    acpi_put32(fadt + 112, 0x000000a5u);
    acpi_fix_table_checksum(fadt);
}

static void acpi_patch_qemu_fadt_io(unsigned char* fadt) {
    unsigned int pm1_evt = acpi_get32(fadt + 56);
    unsigned int pm1_ctl = acpi_get32(fadt + 64);
    unsigned int pm_tmr = acpi_get32(fadt + 76);
    unsigned int gpe0 = acpi_get32(fadt + 80);

    if (pm1_evt < 0x100u) {
        acpi_put32(fadt + 56, QEMU_ACPI_PM_BASE + pm1_evt);
    }
    if (pm1_ctl < 0x100u) {
        acpi_put32(fadt + 64, QEMU_ACPI_PM_BASE + pm1_ctl);
    }
    if (pm_tmr < 0x100u) {
        acpi_put32(fadt + 76, QEMU_ACPI_PM_BASE + pm_tmr);
    }
    if (gpe0 != 0u && gpe0 < 0x100u) {
        acpi_put32(fadt + 80, QEMU_ACPI_PM_BASE + gpe0);
    }
}

static void acpi_build_real_tables(unsigned int base, unsigned int dsdt,
                                   unsigned int dsdt_size) {
    unsigned char* rsdt = (unsigned char*)base;
    unsigned char* fadt = (unsigned char*)(base + 0x0100u);
    unsigned char* facs = (unsigned char*)(base + 0x0200u);
    unsigned int fadt_addr = base + 0x0100u;
    unsigned int facs_addr = base + 0x0200u;

    (void)dsdt_size;
    acpi_build_header(rsdt, "RSDT", 40u, 1u, "ASUS  ", "P2B98-XV");
    acpi_put32(rsdt + 36, fadt_addr);
    acpi_fix_table_checksum(rsdt);

    bios_memset(facs, 0u, 64u);
    acpi_write_bytes(facs, "FACS", 4u);
    acpi_put32(facs + 4, 64u);

    acpi_build_real_fadt(fadt, facs_addr, dsdt);
    acpi_install_rsdp("ASUS  ", base);
}

static int acpi_install_real_dsdt_blob(void) {
    unsigned int base = acpi_table_base();
    unsigned int cap = acpi_table_capacity();
    unsigned int dsdt = base + 0x1000u;
    blob_expand_fn expand = (blob_expand_fn)BLOB_SERVICE_LINEAR;
    struct blob_status status;
    int rc;

    if (bios_dsdt_blob_linear_global == 0u || cap <= 0x1000u) {
        return -1;
    }

    serial_write_string("ACPI real DSDT @ ");
    serial_write_hex32(base);
    serial_write_string("...");
    rc = expand((const void*)bios_dsdt_blob_linear_global,
                (void*)BLOB_STAGE_LINEAR, (void*)dsdt, cap - 0x1000u, &status);
    serial_write_string("\r\n");
    if (rc != 0) {
        serial_write_string("ACPI DSDT blob failed rc=");
        serial_write_hex8((unsigned char)rc);
        serial_write_string(" block=");
        serial_write_hex32(status.block);
        serial_write_string("\r\n");
        return -1;
    }

    acpi_build_real_tables(base, dsdt, status.output_size);
    serial_write_string("ACPI real tables ok\r\n");
    return 0;
}

static void fwcfg_select(unsigned short selector) {
    outw(FW_CFG_PORT_SEL, selector);
}

static unsigned char fwcfg_read8(void) { return inb(FW_CFG_PORT_DATA); }

static unsigned short fwcfg_read_be16(void) {
    unsigned short hi = fwcfg_read8();
    unsigned short lo = fwcfg_read8();
    return (unsigned short)((hi << 8) | lo);
}

static unsigned int fwcfg_read_be32(void) {
    unsigned int b0 = fwcfg_read8();
    unsigned int b1 = fwcfg_read8();
    unsigned int b2 = fwcfg_read8();
    unsigned int b3 = fwcfg_read8();
    return (b0 << 24) | (b1 << 16) | (b2 << 8) | b3;
}

static int fwcfg_signature_ok(void) {
    fwcfg_select(FW_CFG_SIGNATURE);
    return fwcfg_read8() == 'Q' && fwcfg_read8() == 'E' &&
           fwcfg_read8() == 'M' && fwcfg_read8() == 'U';
}

static int fwcfg_find_file(const char* name, unsigned short* selector,
                           unsigned int* size) {
    unsigned int count;
    unsigned int i;

    if (!fwcfg_signature_ok()) {
        return -1;
    }
    fwcfg_select(FW_CFG_FILE_DIR);
    count = fwcfg_read_be32();
    for (i = 0; i < count; ++i) {
        unsigned int file_size = fwcfg_read_be32();
        unsigned short file_select = fwcfg_read_be16();
        char file_name[FW_CFG_MAX_FILE_PATH];
        unsigned int j;

        (void)fwcfg_read_be16();
        for (j = 0; j < FW_CFG_MAX_FILE_PATH; ++j) {
            file_name[j] = (char)fwcfg_read8();
        }
        file_name[FW_CFG_MAX_FILE_PATH - 1u] = '\0';
        if (acpi_name_eq(file_name, name)) {
            *selector = file_select;
            *size = file_size;
            return 0;
        }
    }
    return -1;
}

static void acpi_enable_qemu_pm_io(void) {
    if (pci_read32(0, 1, 3, 0x00) != 0x71138086u) {
        return;
    }
    pci_write32(0, 1, 3, 0x40, QEMU_ACPI_PM_BASE | 0x00000001u);
    pci_write8(0, 1, 3, 0x80, 0x81u);
    pci_write16(0, 1, 3, 0x04,
                (unsigned short)(pci_read16(0, 1, 3, 0x04) | 0x0001u));
}

static void acpi_enable_real_pm_io(void) {
    unsigned short cmd;
    unsigned char misc;

    if (pci_read32(0, ACPI_REAL_PCI_DEV, ACPI_REAL_PCI_FN, 0x00) !=
        0x71138086u) {
        serial_write_string("ACPI real PM dev missing\r\n");
        return;
    }

    pci_write32(0, ACPI_REAL_PCI_DEV, ACPI_REAL_PCI_FN, 0x40,
                ACPI_REAL_PM1_EVT | 0x00000001u);
    misc = pci_read8(0, ACPI_REAL_PCI_DEV, ACPI_REAL_PCI_FN, 0x80);
    pci_write8(0, ACPI_REAL_PCI_DEV, ACPI_REAL_PCI_FN, 0x80,
               (unsigned char)(misc | 0x81u));
    cmd = pci_read16(0, ACPI_REAL_PCI_DEV, ACPI_REAL_PCI_FN, 0x04);
    pci_write16(0, ACPI_REAL_PCI_DEV, ACPI_REAL_PCI_FN, 0x04,
                (unsigned short)(cmd | 0x0001u));

    serial_write_string("ACPI real PM io pmb=");
    serial_write_hex32(
        pci_read32(0, ACPI_REAL_PCI_DEV, ACPI_REAL_PCI_FN, 0x40));
    serial_write_string(" misc=");
    serial_write_hex8(pci_read8(0, ACPI_REAL_PCI_DEV, ACPI_REAL_PCI_FN, 0x80));
    serial_write_string(" cmd=");
    serial_write_hex16(
        pci_read16(0, ACPI_REAL_PCI_DEV, ACPI_REAL_PCI_FN, 0x04));
    serial_write_string("\r\n");
}

static void acpi_enable_real_mode(void) {
    unsigned short cnt = inw((unsigned short)ACPI_REAL_PM1_CNT);
    if ((cnt & ACPI_PM1_CNT_SCI_EN) == 0u) {
        outw((unsigned short)ACPI_REAL_PM1_CNT,
             (unsigned short)(cnt | ACPI_PM1_CNT_SCI_EN));
        cnt = inw((unsigned short)ACPI_REAL_PM1_CNT);
    }
    serial_write_string("ACPI real PM1 cnt=");
    serial_write_hex16(cnt);
    serial_write_string("\r\n");
}

static void fwcfg_read_file(unsigned short selector, void* dst,
                            unsigned int size) {
    unsigned char* p = (unsigned char*)dst;
    unsigned int i;

    fwcfg_select(selector);
    for (i = 0; i < size; ++i) {
        p[i] = fwcfg_read8();
    }
}

static int acpi_should_rsdt_ref(unsigned int sig) {
    if (sig == acpi_sig("RSDT") || sig == acpi_sig("XSDT") ||
        sig == acpi_sig("FACS") || sig == acpi_sig("DSDT")) {
        return 0;
    }
    return 1;
}

static int acpi_patch_qemu_tables(unsigned int base, unsigned int size) {
    struct acpi_table_ref refs[32];
    unsigned int ref_count = 0u;
    unsigned int off = 0u;
    unsigned char* rsdt = 0;
    unsigned char* fadt = 0;
    unsigned int rsdt_len = 0u;
    unsigned int fadt_len = 0u;
    unsigned int dsdt_addr = 0u;
    unsigned int facs_addr = 0u;
    unsigned int i;

    while (off + 36u <= size) {
        unsigned char* table = (unsigned char*)(base + off);
        unsigned int sig = acpi_get32(table);
        unsigned int len = acpi_get32(table + 4);
        if (len < 36u || len > size - off) {
            break;
        }
        if (sig == acpi_sig("RSDT")) {
            rsdt = table;
            rsdt_len = len;
        } else if (sig == acpi_sig("FACP")) {
            fadt = table;
            fadt_len = len;
        } else if (sig == acpi_sig("DSDT")) {
            dsdt_addr = base + off;
        } else if (sig == acpi_sig("FACS")) {
            facs_addr = base + off;
        }

        if (acpi_should_rsdt_ref(sig) &&
            ref_count < sizeof(refs) / sizeof(refs[0])) {
            refs[ref_count].sig = sig;
            refs[ref_count].addr = base + off;
            refs[ref_count].len = len;
            ++ref_count;
        }
        off += len;
    }

    if (rsdt == 0 || fadt == 0 || dsdt_addr == 0u) {
        return -1;
    }

    acpi_patch_qemu_fadt_io(fadt);
    if (fadt_len >= 44u) {
        acpi_put32(fadt + 36, facs_addr);
        acpi_put32(fadt + 40, dsdt_addr);
    }
    if (fadt_len >= 0x94u) {
        acpi_put64(fadt + 0x84u, facs_addr);
        acpi_put64(fadt + 0x8cu, dsdt_addr);
    }
    acpi_fix_table_checksum(fadt);

    if (ref_count > (rsdt_len - 36u) / 4u) {
        ref_count = (rsdt_len - 36u) / 4u;
    }
    acpi_put32(rsdt + 4, 36u + ref_count * 4u);
    for (i = 0; i < ref_count; ++i) {
        acpi_put32(rsdt + 36u + i * 4u, refs[i].addr);
    }
    acpi_fix_table_checksum(rsdt);

    off = 0u;
    while (off + 36u <= size) {
        unsigned char* table = (unsigned char*)(base + off);
        unsigned int sig = acpi_get32(table);
        unsigned int len = acpi_get32(table + 4);
        if (len < 36u || len > size - off) {
            break;
        }
        if (sig != acpi_sig("FACS")) {
            acpi_fix_table_checksum(table);
        }
        off += len;
    }

    acpi_install_rsdp("QEMU  ",
                      base + (unsigned int)(rsdt - (unsigned char*)base));
    return 0;
}

static int acpi_install_qemu_fwcfg(void) {
    unsigned short selector;
    unsigned int size;
    unsigned int base = acpi_table_base();
    unsigned int cap = acpi_table_capacity();

    if (fwcfg_find_file("etc/acpi/tables", &selector, &size) != 0 ||
        size == 0u || size > cap) {
        return -1;
    }
    serial_write_string("ACPI qemu fw_cfg @ ");
    serial_write_hex32(base);
    serial_write_string(" size=");
    serial_write_hex32(size);
    serial_write_string("\r\n");
    acpi_enable_qemu_pm_io();
    fwcfg_read_file(selector, (void*)base, size);
    if (acpi_patch_qemu_tables(base, size) != 0) {
        serial_write_string("ACPI qemu patch failed\r\n");
        return -1;
    }
    serial_write_string("ACPI qemu tables ok\r\n");
    return 0;
}

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

static void acpi_install_for_linux(void) {
    int rc;
    if (bios_qemu_mode) {
        rc = acpi_install_qemu_fwcfg();
    } else {
        rc = acpi_install_real_dsdt_blob();
    }
    if (rc != 0) {
        serial_write_string("ACPI install skipped\r\n");
        return;
    }
    if (bios_qemu_mode) {
        acpi_clear_pm_events(QEMU_ACPI_PM_BASE, QEMU_ACPI_PM_BASE + 0x0cu,
                             ACPI_GPE0_LEN);
    } else {
        acpi_enable_real_pm_io();
        acpi_clear_pm_events(ACPI_REAL_PM1_EVT, ACPI_REAL_GPE0, ACPI_GPE0_LEN);
        acpi_enable_real_mode();
    }
}

#define LINUX_SECTOR_BUF 0x00080000u
#define LINUX_PHDR_BUF 0x00088000u
#define LINUX_BOOT_PARAMS 0x00090000u
#define LINUX_CMDLINE 0x00098000u
#define LINUX_CMDLINE_CAPACITY 2048u
#define LINUX_SERIAL_CMDLINE_TEXT \
    "console=ttyS0,115200n8 earlyprintk=serial,ttyS0,115200"
#define LINUX_ELF_PHDR_MAX 0x4000u
#define TEST_ELF_IMAGE_LINEAR 0x00280000u
#define TEST_ELF_IMAGE_CAPACITY 0x00040000u
#define LINUX_E820_TABLE_OFF 0x02d0u
#define LINUX_E820_MAX 128u
#define ELF32_PT_LOAD 1u
#define VBE_MODE_1024_768_16 0x0117u
#define VBE_MODE_LFB 0x4000u
#define VBE_SUCCESS 0x004fu
#define VBE_ATTR_SUPPORTED 0x0001u
#define VBE_ATTR_GRAPHICS 0x0010u
#define VBE_ATTR_LFB 0x0080u
#define LINUX_VIDEO_TYPE_VLFB 0x23u

struct bios_partition {
    unsigned char boot;
    unsigned char type;
    unsigned int start_lba;
    unsigned int sectors;
};

static unsigned short linux_le16(const unsigned char* p) {
    return (unsigned short)((unsigned short)p[0] | ((unsigned short)p[1] << 8));
}

static unsigned int linux_le32(const unsigned char* p) {
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8) |
           ((unsigned int)p[2] << 16) | ((unsigned int)p[3] << 24);
}

static void linux_put16(unsigned char* p, unsigned short value) {
    p[0] = (unsigned char)value;
    p[1] = (unsigned char)(value >> 8);
}

static void linux_put32(unsigned char* p, unsigned int value) {
    p[0] = (unsigned char)value;
    p[1] = (unsigned char)(value >> 8);
    p[2] = (unsigned char)(value >> 16);
    p[3] = (unsigned char)(value >> 24);
}

static unsigned char* vbe_mode_info(void) {
    return (unsigned char*)(bios16_thunk_runtime_base +
                            (unsigned int)(bios16_vbe_mode_info -
                                           bios16_thunk_start));
}

static unsigned short vbe_info16(unsigned int off) {
    unsigned char* info = vbe_mode_info();
    return (unsigned short)((unsigned short)info[off] |
                            ((unsigned short)info[off + 1u] << 8));
}

static unsigned int vbe_info32(unsigned int off) {
    unsigned char* info = vbe_mode_info();
    return (unsigned int)info[off] | ((unsigned int)info[off + 1u] << 8) |
           ((unsigned int)info[off + 2u] << 16) |
           ((unsigned int)info[off + 3u] << 24);
}

static int vbe_query_mode(unsigned short mode) {
    unsigned char* info = vbe_mode_info();
    unsigned int i;
    unsigned int status;

    for (i = 0u; i < 256u; ++i) {
        info[i] = 0u;
    }
    cache_writeback_invalidate();
    status = bios_call_vbe_mode_info_pm32(mode);
    cache_writeback_invalidate();
    if ((unsigned short)status != VBE_SUCCESS) {
        serial_write_string("VBE 4F01 failed mode=");
        serial_write_hex16(mode);
        serial_write_string(" ax=");
        serial_write_hex16((unsigned short)status);
        serial_write_string("\r\n");
        return -1;
    }
    return 0;
}

static int vbe_set_mode(unsigned short mode) {
    unsigned int status;

    cache_writeback_invalidate();
    status = bios_call_vbe_set_mode_pm32(mode);
    cache_writeback_invalidate();
    if ((unsigned short)status != VBE_SUCCESS) {
        serial_write_string("VBE 4F02 failed mode=");
        serial_write_hex16(mode);
        serial_write_string(" ax=");
        serial_write_hex16((unsigned short)status);
        serial_write_string("\r\n");
        return -1;
    }
    return 0;
}

static int linux_set_vbe_1024x768(void) {
    unsigned short attrs;
    unsigned short width;
    unsigned short height;
    unsigned short pitch;
    unsigned char depth;
    unsigned int base;
    unsigned short mode = VBE_MODE_1024_768_16;

    bios_vbe_lfb_ready = 0u;
    if ((bios_nvram_flags0 & BIOS_NVRAM_FLAGS0_VESA_1024_768) == 0u ||
        bios_vgabios_initialized == 0u || vbe_query_mode(mode) != 0) {
        return -1;
    }

    attrs = vbe_info16(0x00u);
    width = vbe_info16(0x12u);
    height = vbe_info16(0x14u);
    pitch = vbe_info16(0x10u);
    depth = vbe_mode_info()[0x19u];
    base = vbe_info32(0x28u);
    if ((attrs & (VBE_ATTR_SUPPORTED | VBE_ATTR_GRAPHICS | VBE_ATTR_LFB)) !=
            (VBE_ATTR_SUPPORTED | VBE_ATTR_GRAPHICS | VBE_ATTR_LFB) ||
        width != 1024u || height != 768u || depth != 16u || pitch == 0u ||
        base == 0u) {
        serial_write_string("VBE 1024x768x16 unusable attrs=");
        serial_write_hex16(attrs);
        serial_write_string(" wh=");
        serial_write_u32(width);
        serial_write_string("x");
        serial_write_u32(height);
        serial_write_string(" depth=");
        serial_write_u32(depth);
        serial_write_string(" base=");
        serial_write_hex32(base);
        serial_write_string("\r\n");
        return -1;
    }

    if (vbe_set_mode((unsigned short)(mode | VBE_MODE_LFB)) != 0) {
        return -1;
    }

    bios_vbe_lfb_ready = 1u;
    bios_vbe_lfb_width = width;
    bios_vbe_lfb_height = height;
    bios_vbe_lfb_depth = depth;
    bios_vbe_lfb_pitch = pitch;
    bios_vbe_lfb_base = base;
    bios_vbe_lfb_red_size = vbe_mode_info()[0x1fu];
    bios_vbe_lfb_red_pos = vbe_mode_info()[0x20u];
    bios_vbe_lfb_green_size = vbe_mode_info()[0x21u];
    bios_vbe_lfb_green_pos = vbe_mode_info()[0x22u];
    bios_vbe_lfb_blue_size = vbe_mode_info()[0x23u];
    bios_vbe_lfb_blue_pos = vbe_mode_info()[0x24u];
    bios_vbe_lfb_rsvd_size = vbe_mode_info()[0x25u];
    bios_vbe_lfb_rsvd_pos = vbe_mode_info()[0x26u];
    bios_vbe_lfb_pages = vbe_mode_info()[0x1du];
    bios_vbe_lfb_attrs = attrs;

    serial_write_string("VBE mode 1024x768x16 LFB @ ");
    serial_write_hex32(base);
    serial_write_string(" pitch=");
    serial_write_u32(pitch);
    serial_write_string("\r\n");
    return 0;
}

static void linux_apply_vbe_screen_info(void) {
    unsigned char* bp = (unsigned char*)LINUX_BOOT_PARAMS;
    unsigned int lfb_size;

    if (bios_vbe_lfb_ready == 0u) {
        return;
    }
    lfb_size =
        (unsigned int)bios_vbe_lfb_pitch * (unsigned int)bios_vbe_lfb_height;

    bp[0x006u] = (unsigned char)VBE_MODE_1024_768_16;
    bp[0x00fu] = LINUX_VIDEO_TYPE_VLFB;
    linux_put16(bp + 0x012u, bios_vbe_lfb_width);
    linux_put16(bp + 0x014u, bios_vbe_lfb_height);
    linux_put16(bp + 0x016u, bios_vbe_lfb_depth);
    linux_put32(bp + 0x018u, bios_vbe_lfb_base);
    linux_put32(bp + 0x01cu, lfb_size);
    linux_put16(bp + 0x024u, bios_vbe_lfb_pitch);
    bp[0x026u] = bios_vbe_lfb_red_size;
    bp[0x027u] = bios_vbe_lfb_red_pos;
    bp[0x028u] = bios_vbe_lfb_green_size;
    bp[0x029u] = bios_vbe_lfb_green_pos;
    bp[0x02au] = bios_vbe_lfb_blue_size;
    bp[0x02bu] = bios_vbe_lfb_blue_pos;
    bp[0x02cu] = bios_vbe_lfb_rsvd_size;
    bp[0x02du] = bios_vbe_lfb_rsvd_pos;
    linux_put16(bp + 0x032u, bios_vbe_lfb_pages);
    linux_put16(bp + 0x034u, bios_vbe_lfb_attrs);
}

static int linux_sector_is_elf32_i386(const unsigned char* sector) {
    return sector[0] == 0x7fu && sector[1] == 'E' && sector[2] == 'L' &&
           sector[3] == 'F' && sector[4] == 1u && sector[5] == 1u &&
           linux_le16(sector + 0x12u) == 3u;
}

static int linux_use_whole_disk(struct bios_partition* part) {
    struct bios_hdd_geometry geometry;

    if (!bios_hdd_is_present()) {
        return -1;
    }
    bios_hdd_get_geometry(&geometry);
    if (geometry.total_sectors == 0u) {
        return -1;
    }
    part->boot = 0x00u;
    part->type = 0xffu;
    part->start_lba = 0u;
    part->sectors = geometry.total_sectors;
    return 0;
}

static int linux_parse_mbr_partition(unsigned int index,
                                     struct bios_partition* part,
                                     const unsigned char* mbr) {
    const unsigned char* entry;

    if (index >= 4u || mbr[0x01feu] != 0x55u || mbr[0x01ffu] != 0xaau) {
        return -1;
    }

    entry = mbr + 0x01beu + index * 16u;
    part->boot = entry[0];
    part->type = entry[4];
    part->start_lba = linux_le32(entry + 8u);
    part->sectors = linux_le32(entry + 12u);
    if (part->type == 0u || part->start_lba == 0u || part->sectors == 0u) {
        return -1;
    }
    return 0;
}

static int linux_read_partition(unsigned int index,
                                struct bios_partition* part) {
    unsigned char* mbr = (unsigned char*)LINUX_SECTOR_BUF;

    if (!bios_hdd_is_present() || index >= 4u ||
        bios_hdd_read_sectors(0u, 1u, LINUX_SECTOR_BUF) != 0) {
        return -1;
    }
    return linux_parse_mbr_partition(index, part, mbr);
}

static int linux_select_kernel_source(struct bios_partition* part,
                                      unsigned char* whole_disk) {
    unsigned char* sector0 = (unsigned char*)LINUX_SECTOR_BUF;

    *whole_disk = 0u;
    if (!bios_hdd_is_present() ||
        bios_hdd_read_sectors(0u, 1u, LINUX_SECTOR_BUF) != 0) {
        return -1;
    }
    if (linux_sector_is_elf32_i386(sector0)) {
        if (linux_use_whole_disk(part) != 0) {
            return -1;
        }
        *whole_disk = 1u;
        return 0;
    }
    return linux_parse_mbr_partition(bios_linux_vmlinux_partition, part,
                                     sector0);
}

static int linux_partition_contains(const struct bios_partition* part,
                                    unsigned int offset, unsigned int len) {
    unsigned int end;

    if (len == 0u) {
        return 0;
    }
    end = offset + len - 1u;
    if (end < offset) {
        return -1;
    }
    if ((end >> 9) >= part->sectors) {
        return -1;
    }
    return 0;
}

static int linux_read_partition_bytes(const struct bios_partition* part,
                                      unsigned int offset, unsigned int dest,
                                      unsigned int len) {
    unsigned int original_len = len;
    unsigned int start_tsc = 0u;

    if (linux_partition_contains(part, offset, len) != 0) {
        return -1;
    }
    if (original_len >= 0x00100000u) {
        start_tsc = tsc_low();
    }

    while (len != 0u) {
        unsigned int sector_off = offset & 0x1ffu;
        unsigned int chunk;

        if (sector_off == 0u && (dest & 0x1ffu) == 0u && len >= 512u) {
            unsigned int count = len >> 9;
            if (count > 2048u) {
                count = 2048u;
            }
            if (bios_hdd_read_sectors(part->start_lba + (offset >> 9), count,
                                      dest) != 0) {
                return -1;
            }
            chunk = count << 9;
        } else {
            if (bios_hdd_read_sectors(part->start_lba + (offset >> 9), 1u,
                                      LINUX_SECTOR_BUF) != 0) {
                return -1;
            }
            chunk = 512u - sector_off;
            if (chunk > len) {
                chunk = len;
            }
            bios_memcpy((void*)dest,
                        (const void*)(LINUX_SECTOR_BUF + sector_off), chunk);
        }
        offset += chunk;
        dest += chunk;
        len -= chunk;
    }
    if (original_len >= 0x00100000u) {
        unsigned int cycles = tsc_low() - start_tsc;
        serial_write_string("Linux read bytes=");
        serial_write_hex32(original_len);
        serial_write_string(" cycles=");
        serial_write_hex32(cycles);
        serial_write_string("\r\n");
    }
    return 0;
}

static int linux_elf_entry_phys(unsigned int entry, unsigned char* phdrs,
                                unsigned int phnum, unsigned int phentsize,
                                unsigned int* entry_phys) {
    unsigned int i;

    for (i = 0; i < phnum; ++i) {
        unsigned char* ph = phdrs + i * phentsize;
        unsigned int type = linux_le32(ph + 0u);
        unsigned int vaddr = linux_le32(ph + 8u);
        unsigned int paddr = linux_le32(ph + 12u);
        unsigned int memsz = linux_le32(ph + 20u);
        if (type == ELF32_PT_LOAD && entry >= vaddr && entry - vaddr < memsz) {
            *entry_phys = paddr + (entry - vaddr);
            return 0;
        }
    }
    if (entry >= 0x00100000u && entry < bios_extended_usable_end()) {
        *entry_phys = entry;
        return 0;
    }
    return -1;
}

static int linux_resolve_load_phys(unsigned int paddr, unsigned int vaddr,
                                   unsigned int* out) {
    if (paddr >= 0x00100000u) {
        *out = paddr;
        return 0;
    }
    if (vaddr >= 0xc0000000u) {
        *out = vaddr - 0xc0000000u;
        return *out >= 0x00100000u ? 0 : -1;
    }
    if (vaddr >= 0x00100000u) {
        *out = vaddr;
        return 0;
    }
    return -1;
}

static int linux_load_initrd(const struct bios_partition* part,
                             unsigned int load_high, unsigned int* initrd_base,
                             unsigned int* initrd_size) {
    unsigned int size;
    unsigned int base;
    unsigned int usable_end = bios_extended_usable_end();

    if (part->sectors > (usable_end >> 9)) {
        return -1;
    }
    size = part->sectors << 9;
    if (size == 0u || size > (usable_end - 0x00100000u)) {
        return -1;
    }
    base = (usable_end - size) & ~0xfffu;
    if (base < 0x00100000u || base < load_high || base + size > usable_end) {
        return -1;
    }
    if (linux_read_partition_bytes(part, 0u, base, size) != 0) {
        return -1;
    }
    *initrd_base = base;
    *initrd_size = size;
    return 0;
}

static void linux_write_cmdline(void) {
    unsigned int i;
    unsigned char* dst = (unsigned char*)LINUX_CMDLINE;

    i = 0u;
    if ((bios_nvram_flags0 & BIOS_NVRAM_FLAGS0_SERIAL_CONSOLE) != 0u) {
        static const char serial_text[] = LINUX_SERIAL_CMDLINE_TEXT;
        unsigned int j;
        for (j = 0u; serial_text[j] != '\0' &&
                    i + 1u < LINUX_CMDLINE_CAPACITY;
             ++j) {
            dst[i++] = (unsigned char)serial_text[j];
        }
    }
    if (bios_linux_cmdline_suffix[0] != '\0' && i != 0u &&
        i + 1u < LINUX_CMDLINE_CAPACITY) {
        dst[i++] = ' ';
    }
    if (bios_linux_cmdline_suffix[0] != '\0') {
        unsigned int j;
        for (j = 0; bios_linux_cmdline_suffix[j] != '\0' &&
                    i + 1u < LINUX_CMDLINE_CAPACITY;
             ++j) {
            dst[i++] = (unsigned char)bios_linux_cmdline_suffix[j];
        }
    }
    dst[i] = '\0';
}

static void linux_setup_boot_params(unsigned int entry_phys,
                                    unsigned int initrd_base,
                                    unsigned int initrd_size) {
    unsigned char* bp = (unsigned char*)LINUX_BOOT_PARAMS;
    unsigned int count = e820_entry_count();
    unsigned int i;
    unsigned int alt_mem_kb = 0u;
    unsigned int usable_end = bios_extended_usable_end();

    if (count > LINUX_E820_MAX) {
        count = LINUX_E820_MAX;
    }
    if (usable_end > 0x00100000u) {
        alt_mem_kb = (usable_end - 0x00100000u) >> 10;
        if (alt_mem_kb > 0xffffu) {
            alt_mem_kb = 0xffffu;
        }
    }

    bios_memset(bp, 0u, 4096u);
    linux_write_cmdline();

    linux_put16(bp + 0x01e0u, (unsigned short)alt_mem_kb);
    bp[0x01e8u] = (unsigned char)count;
    for (i = 0; i < count; ++i) {
        struct e820_entry entry;
        if (e820_get_entry(i, &entry) != 0) {
            break;
        }
        bios_memcpy(bp + LINUX_E820_TABLE_OFF + i * sizeof(entry), &entry,
                    sizeof(entry));
    }

    linux_put16(bp + 0x01feu, 0xaa55u);
    linux_put32(bp + 0x0202u, 0x53726448u);
    linux_put16(bp + 0x0206u, 0x020fu);
    bp[0x0210u] = 0xffu;
    bp[0x0211u] = 0x80u;
    linux_put16(bp + 0x0224u, 0xe000u);
    linux_put32(bp + 0x0214u, entry_phys);
    linux_put32(bp + 0x0218u, initrd_base);
    linux_put32(bp + 0x021cu, initrd_size);
    linux_put32(bp + 0x0228u, LINUX_CMDLINE);
    linux_put32(bp + 0x022cu, usable_end - 1u);
    linux_put32(bp + 0x0230u, 0x00100000u);
    bp[0x0234u] = 0u;
    linux_put32(bp + 0x0238u, LINUX_CMDLINE_CAPACITY);
}

static void linux_jump(unsigned int entry_phys) {
    cpu_serialize();
    __asm__ volatile(
        "cli\n\t"
        "cld\n\t"
        "movl %0, %%esi\n\t"
        "xorl %%ebp, %%ebp\n\t"
        "jmp *%1"
        :
        : "r"(LINUX_BOOT_PARAMS), "r"(entry_phys)
        : "esi", "ebp", "memory");
    for (;;) {
        __asm__ volatile("hlt");
    }
}

static int test_elf_load_image(unsigned char* elf, unsigned int image_size,
                               unsigned int* entry_phys) {
    unsigned int entry;
    unsigned int phoff;
    unsigned int phentsize;
    unsigned int phnum;
    unsigned int phdr_bytes;
    unsigned int i;

    if (image_size < 52u || !linux_sector_is_elf32_i386(elf)) {
        serial_write_string("Test ELF bad header\r\n");
        return -1;
    }

    entry = linux_le32(elf + 0x18u);
    phoff = linux_le32(elf + 0x1cu);
    phentsize = linux_le16(elf + 0x2au);
    phnum = linux_le16(elf + 0x2cu);
    phdr_bytes = phentsize * phnum;
    if (phentsize < 32u || phnum == 0u || phnum > 128u ||
        phdr_bytes > LINUX_ELF_PHDR_MAX || phoff > image_size ||
        phdr_bytes > image_size - phoff) {
        serial_write_string("Test ELF bad phdr\r\n");
        return -1;
    }

    serial_write_string("Test ELF entry=");
    serial_write_hex32(entry);
    serial_write_string(" phnum=");
    serial_write_u32(phnum);
    serial_write_string("\r\n");

    for (i = 0u; i < phnum; ++i) {
        unsigned char* ph = elf + phoff + i * phentsize;
        unsigned int type = linux_le32(ph + 0u);
        unsigned int off = linux_le32(ph + 4u);
        unsigned int vaddr = linux_le32(ph + 8u);
        unsigned int paddr = linux_le32(ph + 12u);
        unsigned int filesz = linux_le32(ph + 16u);
        unsigned int memsz = linux_le32(ph + 20u);

        if (type != ELF32_PT_LOAD) {
            continue;
        }
        if (linux_resolve_load_phys(paddr, vaddr, &paddr) != 0 ||
            paddr >= bios_extended_usable_end() || filesz > memsz ||
            memsz > bios_extended_usable_end() - paddr ||
            off > image_size || filesz > image_size - off) {
            serial_write_string("Test ELF bad LOAD\r\n");
            return -1;
        }

        serial_write_string("Test LOAD ");
        serial_write_hex32(paddr);
        serial_write_string(" filesz=");
        serial_write_hex32(filesz);
        serial_write_string(" memsz=");
        serial_write_hex32(memsz);
        serial_write_string(" off=");
        serial_write_hex32(off);
        serial_write_string("\r\n");

        bios_memcpy((void*)paddr, elf + off, filesz);
        if (memsz > filesz) {
            bios_memset((void*)(paddr + filesz), 0u, memsz - filesz);
        }
    }

    if (linux_elf_entry_phys(entry, elf + phoff, phnum, phentsize,
                             entry_phys) != 0) {
        serial_write_string("Test ELF entry not loaded\r\n");
        return -1;
    }
    return 0;
}

static void run_test_elf_blob(void) {
    typedef unsigned int (*test_elf_entry_fn)(unsigned int, unsigned int,
                                             unsigned int, unsigned int);
    blob_expand_fn expand = (blob_expand_fn)BLOB_SERVICE_LINEAR;
    struct blob_status status;
    unsigned char* image = (unsigned char*)TEST_ELF_IMAGE_LINEAR;
    unsigned int entry_phys = 0u;
    unsigned int pm1_evt = bios_qemu_mode ? QEMU_ACPI_PM_BASE : ACPI_REAL_PM1_EVT;
    unsigned int pm1_cnt =
        bios_qemu_mode ? (QEMU_ACPI_PM_BASE + 4u) : ACPI_REAL_PM1_CNT;
    unsigned int rc;
    int expand_rc;

    if (bios_test_elf_blob_linear_global == 0u) {
        serial_write_string("No test ELF blob\r\n");
        return;
    }

    serial_write_string("Run ROM test ELF...\r\n");
    expand_rc = expand((const void*)bios_test_elf_blob_linear_global,
                       (void*)BLOB_STAGE_LINEAR, image,
                       TEST_ELF_IMAGE_CAPACITY, &status);
    if (expand_rc != 0) {
        serial_write_string("Test ELF blob failed rc=");
        serial_write_hex8((unsigned char)expand_rc);
        serial_write_string(" block=");
        serial_write_hex32(status.block);
        serial_write_string("\r\n");
        return;
    }

    if (test_elf_load_image(image, status.output_size, &entry_phys) != 0) {
        return;
    }

    storage_scan(bios_total_bytes_global);
    install_bios_thunks();
    install_boot_drive();
    install_pm_stack_top();
    install_vgabios_shadow();
    rtc_prepare_for_linux();
    acpi_install_for_linux();
    linux_setup_boot_params(entry_phys, 0u, 0u);
    init_vgabios_for_linux();
    if (linux_set_vbe_1024x768() == 0) {
        linux_apply_vbe_screen_info();
    }

    serial_write_string("Call test ELF entry=");
    serial_write_hex32(entry_phys);
    serial_write_string(" params=");
    serial_write_hex32(LINUX_BOOT_PARAMS);
    serial_write_string("\r\n");
    cpu_serialize();
    rc = ((test_elf_entry_fn)entry_phys)(LINUX_BOOT_PARAMS, ACPI_RSDP_LINEAR,
                                         pm1_evt, pm1_cnt);
    cpu_serialize();
    serial_write_string("Test ELF returned ");
    serial_write_hex32(rc);
    serial_write_string("\r\n");
}

static void nvram_record_boot_success(unsigned char kind) {
    if (bios_boot_priority != BIOS_NVRAM_BOOT_PRIORITY_AUTO) {
        return;
    }
    if (kind != BIOS_HDD_KIND_IDE && kind != BIOS_HDD_KIND_USB) {
        return;
    }
    bios_boot_priority = kind;
    nvram_save_boot_priority(kind);
    serial_write_string("boot priority learned=");
    serial_write_u32(kind);
    serial_write_string("\r\n");
}

static int try_boot_linux_current(void) {
    struct bios_partition kernel_part;
    struct bios_partition initrd_part;
    unsigned char* ehdr = (unsigned char*)LINUX_SECTOR_BUF;
    unsigned char* phdrs = (unsigned char*)LINUX_PHDR_BUF;
    unsigned int entry;
    unsigned int entry_phys = 0u;
    unsigned int phoff;
    unsigned int phentsize;
    unsigned int phnum;
    unsigned int phdr_bytes;
    unsigned int load_high = 0x00100000u;
    unsigned int initrd_base = 0u;
    unsigned int initrd_size = 0u;
    unsigned int i;
    unsigned char whole_disk = 0u;

    if (linux_select_kernel_source(&kernel_part, &whole_disk) != 0) {
        return 0;
    }
    if (whole_disk) {
        serial_write_string("Linux disk start=");
    } else {
        serial_write_string("Linux part");
        serial_write_u32((unsigned int)bios_linux_vmlinux_partition + 1u);
        serial_write_string(" start=");
    }
    serial_write_hex32(kernel_part.start_lba);
    serial_write_string(" size=");
    serial_write_hex32(kernel_part.sectors);
    if (!whole_disk) {
        serial_write_string(" type=");
        serial_write_hex8(kernel_part.type);
    }
    serial_write_string("\r\n");

    if (linux_read_partition_bytes(&kernel_part, 0u, LINUX_SECTOR_BUF, 512u) !=
        0) {
        return 0;
    }
    if (!linux_sector_is_elf32_i386(ehdr)) {
        serial_write_string("Linux kernel is not ELF32 i386\r\n");
        return 0;
    }

    entry = linux_le32(ehdr + 0x18u);
    phoff = linux_le32(ehdr + 0x1cu);
    phentsize = linux_le16(ehdr + 0x2au);
    phnum = linux_le16(ehdr + 0x2cu);
    phdr_bytes = phentsize * phnum;
    if (phentsize < 32u || phnum == 0u || phnum > 128u ||
        phdr_bytes > LINUX_ELF_PHDR_MAX ||
        linux_read_partition_bytes(&kernel_part, phoff, LINUX_PHDR_BUF,
                                   phdr_bytes) != 0) {
        serial_write_string("Linux bad ELF phdr\r\n");
        return 0;
    }

    serial_write_string("Linux ELF entry=");
    serial_write_hex32(entry);
    serial_write_string(" phnum=");
    serial_write_u32(phnum);
    serial_write_string("\r\n");

    for (i = 0; i < phnum; ++i) {
        unsigned char* ph = phdrs + i * phentsize;
        unsigned int type = linux_le32(ph + 0u);
        unsigned int off = linux_le32(ph + 4u);
        unsigned int vaddr = linux_le32(ph + 8u);
        unsigned int paddr = linux_le32(ph + 12u);
        unsigned int filesz = linux_le32(ph + 16u);
        unsigned int memsz = linux_le32(ph + 20u);

        if (type != ELF32_PT_LOAD) {
            continue;
        }
        if (linux_resolve_load_phys(paddr, vaddr, &paddr) != 0 ||
            paddr >= bios_extended_usable_end() || filesz > memsz ||
            memsz > bios_extended_usable_end() - paddr ||
            linux_partition_contains(&kernel_part, off, filesz) != 0) {
            serial_write_string("Linux bad LOAD\r\n");
            return 0;
        }
        linux_put32(ph + 12u, paddr);

        serial_write_string("Linux LOAD ");
        serial_write_hex32(paddr);
        serial_write_string(" filesz=");
        serial_write_hex32(filesz);
        serial_write_string(" memsz=");
        serial_write_hex32(memsz);
        serial_write_string(" off=");
        serial_write_hex32(off);
        serial_write_string("\r\n");

        if (linux_read_partition_bytes(&kernel_part, off, paddr, filesz) != 0) {
            serial_write_string("Linux LOAD read failed\r\n");
            return 0;
        }
        if (memsz > filesz) {
            bios_memset((void*)(paddr + filesz), 0u, memsz - filesz);
        }
        if (paddr + memsz > load_high) {
            load_high = paddr + memsz;
        }
    }

    if (linux_elf_entry_phys(entry, phdrs, phnum, phentsize, &entry_phys) !=
        0) {
        serial_write_string("Linux entry not loaded\r\n");
        return 0;
    }

    if (!whole_disk && bios_linux_vmlinux_partition != 1u &&
        linux_read_partition(1u, &initrd_part) == 0 &&
        linux_load_initrd(&initrd_part, load_high, &initrd_base,
                          &initrd_size) == 0) {
        serial_write_string("Linux initrd @ ");
        serial_write_hex32(initrd_base);
        serial_write_string(" size=");
        serial_write_hex32(initrd_size);
        serial_write_string("\r\n");
    } else {
        serial_write_string("Linux initrd: none\r\n");
    }

    rtc_prepare_for_linux();
    acpi_install_for_linux();
    linux_setup_boot_params(entry_phys, initrd_base, initrd_size);
    init_vgabios_for_linux();
    if (linux_set_vbe_1024x768() == 0) {
        linux_apply_vbe_screen_info();
    }
    nvram_record_boot_success(bios_hdd_current_kind());
    serial_write_string("Boot Linux entry=");
    serial_write_hex32(entry_phys);
    serial_write_string(" params=");
    serial_write_hex32(LINUX_BOOT_PARAMS);
    serial_write_string("\r\n");
    linux_jump(entry_phys);
    return 1;
}

static int try_boot_linux(void) {
    if (bios_boot_priority == BIOS_NVRAM_BOOT_PRIORITY_IDE) {
        if (bios_hdd_select_kind(BIOS_HDD_KIND_IDE) == 0u) {
            return 0;
        }
        return try_boot_linux_current();
    }
    if (bios_boot_priority == BIOS_NVRAM_BOOT_PRIORITY_USB) {
        if (bios_hdd_select_kind(BIOS_HDD_KIND_USB) == 0u) {
            return 0;
        }
        return try_boot_linux_current();
    }

    if (bios_hdd_select_kind(BIOS_HDD_KIND_IDE) != 0u &&
        try_boot_linux_current()) {
        return 1;
    }
    if (bios_hdd_select_kind(BIOS_HDD_KIND_USB) != 0u &&
        try_boot_linux_current()) {
        return 1;
    }
    return 0;
}

static void bios_int15_e820(struct rm_int13_frame* f) {
    unsigned int index = f->ebx32;
    unsigned int count = e820_entry_count();
    struct e820_entry* entry;

    if (f->edx32 != E820_SMAP || f->ecx32 < sizeof(*entry) || index >= count) {
        rm_return_eax32(f, 0x00008600u);
        rm_set_cf(f);
        return;
    }

    entry = (struct e820_entry*)rm_seg_off_to_linear(f->es, f->di);
    if (e820_get_entry(index, entry) != 0) {
        rm_return_eax32(f, 0x00008600u);
        rm_set_cf(f);
        return;
    }

    rm_return_eax32(f, E820_SMAP);
    rm_return_edx32(f, E820_SMAP);
    rm_return_ecx32(f, sizeof(*entry));
    rm_return_ebx32(f, (index + 1u < count) ? index + 1u : 0u);
    rm_clear_cf(f);
}

static void bios_int13_hdd_params(struct rm_int13_frame* f) {
    struct bios_hdd_geometry geometry;
    unsigned int max_cyl;
    unsigned int max_head;
    unsigned int spt;

    bios_hdd_get_geometry(&geometry);
    max_cyl = geometry.cylinders - 1u;
    max_head = geometry.heads - 1u;
    spt = geometry.sectors_per_track;

    f->ax = 0u;
    f->cx = (unsigned short)(((max_cyl & 0xffu) << 8) | spt |
                             ((max_cyl >> 2) & 0xc0u));
    f->dx = (unsigned short)((max_head << 8) | 0x01u);
    rm_clear_cf(f);
}

static void bios_int13_hdd_chs_rw(struct rm_int13_frame* f, unsigned char ah) {
    struct bios_hdd_geometry geometry;
    unsigned int count = (unsigned char)f->ax;
    unsigned int cylinder =
        ((unsigned int)(f->cx >> 8) | (((unsigned int)f->cx & 0x00c0u) << 2));
    unsigned int sector = (unsigned int)(f->cx & 0x003fu);
    unsigned int head = (unsigned int)(f->dx >> 8);
    unsigned int lba;
    unsigned int dest;

    bios_hdd_get_geometry(&geometry);
    if (ah != 0x02u || sector == 0u || count == 0u || head >= geometry.heads ||
        cylinder >= geometry.cylinders) {
        f->ax = 0x0100u;
        rm_set_cf(f);
        return;
    }

    lba = ((cylinder * geometry.heads) + head) * geometry.sectors_per_track +
          sector - 1u;
    dest = rm_seg_off_to_linear(f->es, f->bx);
    if (bios_hdd_read_sectors(lba, count, dest) != 0) {
        f->ax = 0x0100u;
        rm_set_cf(f);
        return;
    }
    f->ax = (unsigned short)count;
    rm_clear_cf(f);
}

static void bios_int13_hdd_ext_read(struct rm_int13_frame* f) {
    struct rm_dap* dap = (struct rm_dap*)rm_seg_off_to_linear(f->ds, f->si);
    unsigned int dest;

    if (dap->size < 0x10u || dap->lba_high != 0u || dap->count == 0u) {
        serial_write_string("13h:42 bad dap size=");
        serial_write_hex8(dap->size);
        serial_write_string(" cnt=");
        serial_write_hex16(dap->count);
        serial_write_string(" high=");
        serial_write_hex32(dap->lba_high);
        serial_write_string("\r\n");
        f->ax = 0x0100u;
        rm_set_cf(f);
        return;
    }
    dest = rm_seg_off_to_linear(dap->seg, dap->off);
    if (bios_hdd_read_sectors(dap->lba_low, dap->count, dest) != 0) {
        f->ax = 0x0100u;
        rm_set_cf(f);
        return;
    }
    f->ax = 0u;
    rm_clear_cf(f);
}

static void bios_int13_hdd_ext_params(struct rm_int13_frame* f) {
    struct rm_edd_params* p =
        (struct rm_edd_params*)rm_seg_off_to_linear(f->ds, f->si);
    struct bios_hdd_geometry geometry;
    unsigned int fill_size;

    bios_hdd_get_geometry(&geometry);
    if (p->size < 0x1au) {
        f->ax = 0x0100u;
        rm_set_cf(f);
        return;
    }

    fill_size = p->size;
    if (fill_size > sizeof(*p)) {
        fill_size = sizeof(*p);
    }
    bios_memset(p, 0, fill_size);
    p->size = (unsigned short)fill_size;
    p->information = 0x0001u;
    p->cylinders = geometry.cylinders;
    p->heads = geometry.heads;
    p->sectors = geometry.sectors_per_track;
    p->total_sectors_low = geometry.total_sectors;
    p->total_sectors_high = 0u;
    p->bytes_per_sector = 512u;
    if (fill_size >= sizeof(*p)) {
        p->edd_config_params = 0u;
    }
    f->ax = 0u;
    rm_clear_cf(f);
}

static void bios_int13_hdd_service(struct rm_int13_frame* f) {
    unsigned char ah = (unsigned char)(f->ax >> 8);
    unsigned char dl = (unsigned char)f->dx;
    struct bios_hdd_geometry geometry;

    if (dl != 0x80u || !bios_hdd_is_present()) {
        f->ax = 0x0100u;
        rm_set_cf(f);
        return;
    }
    bios_hdd_get_geometry(&geometry);

    switch (ah) {
        case 0x00u:
            f->ax &= 0x00ffu;
            rm_clear_cf(f);
            return;
        case 0x08u:
            bios_int13_hdd_params(f);
            return;
        case 0x15u:
            f->ax = (unsigned short)((0x03u << 8) | (f->ax & 0x00ffu));
            f->cx = (unsigned short)(geometry.total_sectors >> 16);
            f->dx = (unsigned short)geometry.total_sectors;
            rm_clear_cf(f);
            return;
        case 0x02u:
        case 0x03u:
            bios_int13_hdd_chs_rw(f, ah);
            return;
        case 0x41u:
            if (f->bx != 0x55aau) {
                f->ax = 0x0100u;
                rm_set_cf(f);
                return;
            }
            f->bx = 0xaa55u;
            f->cx = 0x0001u;
            f->ax = 0x3000u;
            rm_clear_cf(f);
            return;
        case 0x42u:
            bios_int13_hdd_ext_read(f);
            return;
        case 0x48u:
            bios_int13_hdd_ext_params(f);
            return;
        default:
            f->ax = 0x0100u;
            rm_set_cf(f);
            return;
    }
}

static void bios_int13_service(struct rm_int13_frame* f) {
    unsigned char dl = (unsigned char)(f->dx & 0xffu);

    if (dl >= 0x80u) {
        bios_int13_hdd_service(f);
        return;
    }

    f->ax = 0x0100u;
    rm_set_cf(f);
}

void bios_rm_service(unsigned int vector, struct rm_int13_frame* f) {
    bios_update_tick_counter();
    if (0 && vector != 0x16 && vector != 0x10) {
        serial_write_string("[");
        serial_write_hex32(bios_tick_counter);
        serial_write_string("] ");
        serial_write_string("bios_rm_service=");
        serial_write_hex8(vector & 0xffu);
        serial_write_string(", ah=");
        serial_write_hex8(f->ax >> 8);
        serial_write_string("\r\n");
    }
    switch (vector & 0xffu) {
        case 0x10:
            if ((unsigned char)(f->ax >> 8) == 0x0eu) {
                serial_write_char((char)(f->ax & 0xffu));
                bios_tty_advance((unsigned char)(f->ax & 0xffu));
                return;
            }
            if ((unsigned char)(f->ax >> 8) == 0x02u) {
                bios_set_cursor((unsigned char)(f->bx >> 8),
                                (unsigned char)(f->dx >> 8),
                                (unsigned char)(f->dx & 0x00ffu));
                return;
            }
            if ((unsigned char)(f->ax >> 8) == 0x03u) {
                f->dx = bios_get_cursor((unsigned char)(f->bx >> 8));
                f->cx = *(volatile unsigned short*)BDA_CURSOR_SHAPE;
                return;
            }
            if ((unsigned char)(f->ax >> 8) == 0x0fu) {
                f->ax = (unsigned short)((80u << 8) | 0x03u);
                f->bx &= 0x00ffu;
            }
            return;
        case 0x11:
            f->ax = 0x0000u;
            return;
        case 0x12:
            f->ax = bios_dos_base_mem_kb;
            return;
        case 0x13:
        case 0x40:
            bios_int13_service(f);
            return;
        case 0x15:
            if (f->ax == 0xe820u) {
                bios_int15_e820(f);
                return;
            }
            if ((unsigned char)(f->ax >> 8) == 0x88u) {
                unsigned int kb = 0;
                unsigned int usable_end = bios_extended_usable_end();
                if (usable_end > 0x00100000u) {
                    kb = (usable_end - 0x00100000u) >> 10;
                    if (kb > 0xffffu) {
                        kb = 0xffffu;
                    }
                }
                f->ax = (unsigned short)kb;
                rm_clear_cf(f);
                return;
            }
            if (f->ax == 0xe801u) {
                unsigned int usable_end = bios_extended_usable_end();
                unsigned int below16m_kb = 0;
                unsigned int above16m_64k = 0;
                if (usable_end > 0x00100000u) {
                    unsigned int below_end = usable_end;
                    if (below_end > 0x01000000u) {
                        below_end = 0x01000000u;
                    }
                    below16m_kb = (below_end - 0x00100000u) >> 10;
                }
                if (usable_end > 0x01000000u) {
                    above16m_64k = (usable_end - 0x01000000u) >> 16;
                }
                f->ax = (unsigned short)below16m_kb;
                f->bx = (unsigned short)above16m_64k;
                f->cx = (unsigned short)below16m_kb;
                f->dx = (unsigned short)above16m_64k;
                rm_clear_cf(f);
                return;
            }
            if ((unsigned char)(f->ax >> 8) == 0x86u) {
                rm_clear_cf(f);
                return;
            }
            f->ax = (unsigned short)((0x86u << 8) | (f->ax & 0x00ffu));
            rm_set_cf(f);
            return;
        case 0x16:
            switch ((unsigned char)(f->ax >> 8)) {
                case 0x01u:
                case 0x11u:
                    if (!bios_try_fill_keybuf()) {
                        rm_set_zf(f);
                    } else {
                        rm_clear_zf(f);
                        f->ax = *(
                            volatile unsigned short*)(BDA_KBD_BUF_BASE +
                                                      (*(volatile unsigned short*)
                                                           BDA_KBD_HEAD -
                                                       0x001eu));
                    }
                    rm_clear_cf(f);
                    return;
                case 0x02u:
                    f->ax = (unsigned short)((f->ax & 0xff00u) |
                                             *(volatile unsigned char*)
                                                 BDA_KBD_FLAGS1);
                    rm_clear_cf(f);
                    return;
                case 0x12u:
                    f->ax = (unsigned short)((*(volatile unsigned char*)
                                                  BDA_KBD_FLAGS2
                                              << 8) |
                                             *(volatile unsigned char*)
                                                 BDA_KBD_FLAGS1);
                    rm_clear_cf(f);
                    return;
                case 0x00u:
                case 0x10u:
                default:
                    while (!bios_try_fill_keybuf()) {
                    }
                    f->ax = bios_kbd_dequeue();
                    rm_clear_cf(f);
                    return;
            }
        case 0x17:
            rm_set_cf(f);
            return;
        case 0x19:
            prepare_boot_sector();
            install_boot_drive();
            bios_boot_freedos_pm32();
            return;
        case 0x1a:
            switch ((unsigned char)(f->ax >> 8)) {
                case 0x00u:
                    f->cx = (unsigned short)(bios_tick_counter >> 16);
                    f->dx = (unsigned short)bios_tick_counter;
                    f->ax = (unsigned short)(*(
                        volatile unsigned char*)BDA_MIDNIGHT_FLAG);
                    *(volatile unsigned char*)BDA_MIDNIGHT_FLAG = 0u;
                    rm_clear_cf(f);
                    return;
                case 0x01u:
                    bios_set_tick_counter(((unsigned int)f->cx << 16) | f->dx);
                    *(volatile unsigned char*)BDA_MIDNIGHT_FLAG = 0u;
                    rm_clear_cf(f);
                    return;
                case 0x02u: {
                    unsigned char hour_bcd;
                    unsigned char min_bcd;
                    unsigned char sec_bcd;
                    // rtc_dump_raw("GETTIME");
                    if (rtc_read_time_bcd(&hour_bcd, &min_bcd, &sec_bcd) != 0) {
                        hour_bcd = 0x12u;
                        min_bcd = 0x00u;
                        sec_bcd = 0x00u;
                    }
                    f->cx = (unsigned short)(((unsigned short)hour_bcd << 8) |
                                             min_bcd);
                    f->dx = (unsigned short)(((unsigned short)sec_bcd << 8) |
                                             0x00u);
                    rm_clear_cf(f);
                    return;
                }
                case 0x04u: {
                    unsigned char day_bcd;
                    unsigned char mon_bcd;
                    unsigned char year_bcd;
                    unsigned char century = 0x20u;
                    // rtc_dump_raw("GETDATE");
                    if (rtc_read_date_bcd(&year_bcd, &mon_bcd, &day_bcd) != 0) {
                        year_bcd = 0x26u;
                        mon_bcd = 0x05u;
                        day_bcd = 0x08u;
                    }
                    f->cx = (unsigned short)(((unsigned short)century << 8) |
                                             year_bcd);
                    f->dx = (unsigned short)(((unsigned short)mon_bcd << 8) |
                                             day_bcd);
                    rm_clear_cf(f);
                    return;
                }
                case 0x03u:
                    // rtc_dump_raw("SETTIME-BEFORE");
                    if (rtc_set_time_bcd((unsigned char)(f->cx >> 8),
                                         (unsigned char)f->cx,
                                         (unsigned char)(f->dx >> 8)) != 0) {
                        f->ax =
                            (unsigned short)((0x86u << 8) | (f->ax & 0x00ffu));
                        rm_set_cf(f);
                    } else {
                        // rtc_dump_raw("SETTIME-AFTER");
                        rm_clear_cf(f);
                    }
                    return;
                case 0x05u:
                    // rtc_dump_raw("SETDATE-BEFORE");
                    if (rtc_set_date_bcd((unsigned char)f->cx,
                                         (unsigned char)(f->dx >> 8),
                                         (unsigned char)f->dx) != 0) {
                        f->ax =
                            (unsigned short)((0x86u << 8) | (f->ax & 0x00ffu));
                        rm_set_cf(f);
                    } else {
                        // rtc_dump_raw("SETDATE-AFTER");
                        rm_clear_cf(f);
                    }
                    return;
                default:
                    f->ax = (unsigned short)((0x86u << 8) | (f->ax & 0x00ffu));
                    rm_set_cf(f);
                    return;
            }
            rm_set_cf(f);
            return;
        case 0x60: {
            unsigned int linear =
                ((unsigned int)f->cx << 16) | (unsigned int)f->dx;
            switch ((unsigned char)(f->ax >> 8)) {
                case 0x00u:
                    f->ax =
                        (unsigned short)((f->ax & 0xff00u) |
                                         (*(volatile unsigned char*)linear));
                    rm_clear_cf(f);
                    return;
                case 0x01u:
                    *(volatile unsigned char*)linear =
                        (unsigned char)(f->ax & 0x00ffu);
                    rm_clear_cf(f);
                    return;
                default:
                    f->ax = (unsigned short)((0x86u << 8) | (f->ax & 0x00ffu));
                    rm_set_cf(f);
                    return;
            }
        }
        default:
            f->ax = (unsigned short)((0x86u << 8) | (f->ax & 0x00ffu));
            rm_set_cf(f);
            return;
    }
}

static unsigned int tsc_low(void) {
    unsigned int value;
    __asm__ volatile("rdtsc" : "=a"(value) : : "edx");
    return value;
}

static unsigned int bandwidth_read_x100(volatile unsigned int* base,
                                        unsigned int bytes, unsigned int passes,
                                        volatile unsigned int* checksum_out) {
    unsigned int start;
    unsigned int end;
    unsigned int sum = 0;
    unsigned int pass;
    unsigned int offset;
    unsigned int words = bytes / 4u;
    unsigned int limit = words & ~7u;
    unsigned int tail = limit;
    unsigned int s0 = 0;
    unsigned int s1 = 0;
    unsigned int s2 = 0;
    unsigned int s3 = 0;
    unsigned int s4 = 0;
    unsigned int s5 = 0;
    unsigned int s6 = 0;
    unsigned int s7 = 0;

    for (offset = 0; offset < words; ++offset) {
        base[offset] = 0x13579bdfu ^ offset;
    }

    start = tsc_low();
    for (pass = 0; pass < passes; ++pass) {
        for (offset = 0; offset < limit; offset += 8) {
            s0 += base[offset + 0];
            s1 += base[offset + 1];
            s2 += base[offset + 2];
            s3 += base[offset + 3];
            s4 += base[offset + 4];
            s5 += base[offset + 5];
            s6 += base[offset + 6];
            s7 += base[offset + 7];
        }
        for (offset = tail; offset < words; ++offset) {
            sum += base[offset];
        }
    }
    end = tsc_low();

    sum += s0 + s1 + s2 + s3 + s4 + s5 + s6 + s7;
    *checksum_out = sum;
    if (end == start) {
        return 0;
    }
    return ((bytes * passes) * 100u) / (end - start);
}

static void bandwidth_benchmarks(unsigned int total_bytes) {
    volatile unsigned int* l1 = (volatile unsigned int*)0x00400000u;
    volatile unsigned int* l2 = (volatile unsigned int*)0x00410000u;
    volatile unsigned int* dram = (volatile unsigned int*)0x00800000u;
    volatile unsigned int checksum = 0;
    unsigned int bw;

    if (total_bytes < 0x00c00000u) {
        serial_write_string("Bandwidth: skipped\r\n");
        return;
    }

    serial_write_string("Bandwidth (read, B/cycle):\r\n");
    bw = bandwidth_read_x100(l1, 16u * 1024u, 2048u, &checksum);
    serial_write_string("L1  16KiB: ");
    serial_write_fixed2(bw);
    serial_write_string(" (");
    serial_write_hex16((unsigned short)checksum);
    serial_write_string(")\r\n");

    bw = bandwidth_read_x100(l2, 256u * 1024u, 128u, &checksum);
    serial_write_string("L2 256KiB: ");
    serial_write_fixed2(bw);
    serial_write_string(" (");
    serial_write_hex16((unsigned short)checksum);
    serial_write_string(")\r\n");

    bw = bandwidth_read_x100(dram, 4u * 1024u * 1024u, 8u, &checksum);
    serial_write_string("DRAM   4MiB: ");
    serial_write_fixed2(bw);
    serial_write_string(" (");
    serial_write_hex16((unsigned short)checksum);
    serial_write_string(")\r\n");
}

void postcar_resume(unsigned int total_bytes, unsigned int aux_blob_linear) {
    volatile unsigned int stack_cookie = 0x13579bdfu;

    bios_total_bytes_global = total_bytes;
    if (aux_blob_linear != 0u) {
        const unsigned int* aux = (const unsigned int*)aux_blob_linear;
        bios_dsdt_blob_linear_global = aux[BOOT_AUX_DSDT_BLOB];
        bios_vgabios_blob_linear_global = aux[BOOT_AUX_VBIOS_BLOB];
        bios_test_elf_blob_linear_global = aux[BOOT_AUX_TEST_ELF_BLOB];
        bios_maintenance_requested = aux[BOOT_AUX_MAINTENANCE] != 0u ? 1u : 0u;
        bios_shadow_ready = aux[BOOT_AUX_SHADOW_READY] != 0u ? 1u : 0u;
    } else {
        bios_dsdt_blob_linear_global = 0u;
        bios_vgabios_blob_linear_global = 0u;
        bios_test_elf_blob_linear_global = 0u;
        bios_maintenance_requested = 0u;
        bios_shadow_ready = 0u;
    }
    bios_qemu_mode = 0u;
    storage_set_scratch_base(bios_top_reserved_base());
    nvram_load_settings();
    outb(0x80, POST_DRAM_STACK);
    serial_write_string("BIOS.elf @ 000f0000\r\n");
    serial_write_string("post-CAR ok\r\n");
    serial_write_string("DRAM stack @ ");
    serial_write_hex16((unsigned short)(((unsigned int)&stack_cookie) >> 16));
    serial_write_hex16((unsigned short)((unsigned int)&stack_cookie));
    serial_write_string("\r\n");
    serial_write_string("Usable DRAM: ");
    serial_write_u32(total_bytes >> 10);
    serial_write_string("K\r\n");
    bios_run_optional_memtest();
    if (bios_run_test_blob != 0u) {
        nvram_consume_test_blob_request();
        run_test_elf_blob();
        serial_write_string("Test blob halted\r\n");
        for (;;) {
            __asm__ volatile("hlt");
        }
    }
    if (bios_maintenance_requested != 0u) {
        maintenance_prompt();
    }
    storage_scan(total_bytes);
    install_bios_thunks();
    install_boot_drive();
    install_pm_stack_top();
    install_vgabios_shadow();
    serial_write_string("IVT thunks installed @ ");
    serial_write_hex32(bios16_thunk_runtime_base);
    serial_write_string("\r\n");
    if (try_boot_linux()) {
        for (;;) {
            __asm__ volatile("hlt");
        }
    }
    prepare_boot_sector();
    install_boot_drive();
    serial_write_string("Booting drive=");
    serial_write_hex8(bios_boot_drive);
    serial_write_string("...\r\n");
    bios_boot_freedos_pm32();
    serial_write_string("FreeDOS returned\r\n");
    bandwidth_benchmarks(total_bytes);
    for (;;) {
        __asm__ volatile("hlt");
    }
}

void bios32_entry_c(unsigned int total_bytes, unsigned int aux_blob_linear) {
    zero_bss();
    postcar_resume(total_bytes, aux_blob_linear);
}

void bios32_qemu_entry(unsigned int total_bytes, unsigned int aux_blob_linear) {
    volatile unsigned int stack_cookie = 0x2468ace0u;

    zero_bss();
    bios_total_bytes_global = total_bytes;
    if (aux_blob_linear != 0u) {
        const unsigned int* aux = (const unsigned int*)aux_blob_linear;
        bios_dsdt_blob_linear_global = aux[BOOT_AUX_DSDT_BLOB];
        bios_vgabios_blob_linear_global = aux[BOOT_AUX_VBIOS_BLOB];
        bios_test_elf_blob_linear_global = aux[BOOT_AUX_TEST_ELF_BLOB];
        bios_maintenance_requested = aux[BOOT_AUX_MAINTENANCE] != 0u ? 1u : 0u;
        bios_shadow_ready = aux[BOOT_AUX_SHADOW_READY] != 0u ? 1u : 0u;
    } else {
        bios_dsdt_blob_linear_global = 0u;
        bios_vgabios_blob_linear_global = 0u;
        bios_test_elf_blob_linear_global = 0u;
        bios_maintenance_requested = 0u;
        bios_shadow_ready = 0u;
    }
    bios_qemu_mode = 1u;
    storage_set_scratch_base(bios_top_reserved_base());
    nvram_load_settings();
    serial_write_string("QEMU BIOS.elf @ 000f0000\r\n");
    serial_write_string("QEMU stack @ ");
    serial_write_hex32((unsigned int)&stack_cookie);
    serial_write_string("\r\n");
    bios_run_optional_memtest();
    if (bios_run_test_blob != 0u) {
        nvram_consume_test_blob_request();
        run_test_elf_blob();
        serial_write_string("QEMU test blob halted\r\n");
        for (;;) {
            __asm__ volatile("hlt");
        }
    }
    if (bios_maintenance_requested != 0u) {
        maintenance_prompt();
    }
    storage_scan(total_bytes);
    install_bios_thunks();
    install_boot_drive();
    install_pm_stack_top();
    install_vgabios_shadow();
    if (try_boot_linux()) {
        for (;;) {
            __asm__ volatile("hlt");
        }
    }
    prepare_boot_sector();
    install_boot_drive();
    serial_write_string("QEMU boot drive=");
    serial_write_hex8(bios_boot_drive);
    serial_write_string("...\r\n");
    bios_boot_freedos_pm32();
    serial_write_string("QEMU FreeDOS returned\r\n");
    bandwidth_benchmarks(total_bytes);
    for (;;) {
        __asm__ volatile("hlt");
    }
}
