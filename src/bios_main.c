#include "acpi_tables.h"
#include "bios_io.h"
#include "bios_memory.h"
#include "bios_nvram.h"
#include "bios_pci.h"
#include "bios_rtc.h"
#include "bios_serial.h"
#include "bios_storage.h"
#include "blob.h"
#include "service_table.h"
#include "app/linux_loader/linux_loader.h"
#include "app/legacy/legacy_boot.h"
#include "app/legacy/legacy_floppy.h"
#include "app/legacy/legacy_runtime.h"
#include "app/legacy/legacy_thunk.h"
#include "post_code.h"

void bios32_entry_c(unsigned int total_bytes, unsigned int aux_blob_linear);
extern void bios_boot_freedos_pm32(void);
extern void bios_call_vgabios_init_pm32(void);
extern unsigned int bios_call_vbe_mode_info_pm32(unsigned int mode);
extern unsigned int bios_call_vbe_set_mode_pm32(unsigned int mode);
extern unsigned char __bss_start[];
extern unsigned char __bss_end[];

static unsigned int bios_total_bytes_global = 0;
static unsigned int bios_vgabios_blob_linear_global = 0;
static unsigned int bios_test_elf_blob_linear_global = 0;
static unsigned int bios_rsdp_linear_global = 0;
static unsigned int bios_acpi_pm1_evt_global = 0;
static unsigned int bios_acpi_pm1_cnt_global = 0;
static unsigned int bios_acpi_gpe0_global = 0;
static unsigned int bios_acpi_gpe0_len_global = 0;
static unsigned int bios_acpi_flags_global = 0;
static struct shared_service_table* bios_shared_service_global = 0;
static unsigned char bios_vgabios_shadow_ready = 0;
static unsigned char bios_vgabios_initialized = 0;
static unsigned char bios_maintenance_requested = 0;
static unsigned char bios_shadow_ready = 0;
static unsigned char bios_nvram_flags0 = BIOS_NVRAM_FLAGS0_DEFAULT;
static unsigned char bios_boot_priority = BIOS_NVRAM_BOOT_PRIORITY_DEFAULT;
static unsigned char bios_linux_vmlinux_partition = 0;
static unsigned char bios_enable_memtest = 0;
static unsigned char bios_run_test_blob = 0;
static char bios_linux_cmdline_suffix[BIOS_NVRAM_CMDLINE_MAX];
static const unsigned int bios_runtime_gdt_linear = 0x000ff800u;
static const unsigned short bios_ebda_segment = 0x0000u;
static const unsigned short bios_dos_base_mem_kb = 640u;
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
static void acpi_install_for_linux(void);

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

static blob_expand_fn bios_blob_expand_fn(void) {
    if (bios_shared_service_global != 0 &&
        bios_shared_service_global->blob_expand != 0u) {
        return (blob_expand_fn)bios_shared_service_global->blob_expand;
    }
    return 0;
}

static void install_vgabios_shadow(void) {
    blob_expand_fn expand = bios_blob_expand_fn();
    struct blob_status status;
    unsigned int size;
    unsigned int i;
    unsigned char sum = 0u;
    int rc;

    bios_vgabios_shadow_ready = 0u;
    if (bios_vgabios_blob_linear_global == 0u) {
        return;
    }
    if (expand == 0) {
        serial_write_string("VBIOS blob service missing\r\n");
        return;
    }

    serial_write_string("VBIOS @ 000c0000...");
    rc = expand((const void*)bios_vgabios_blob_linear_global,
                (void*)BLOB_STAGE_LINEAR, (void*)VGA_BIOS_LINEAR,
                VGA_BIOS_CAPACITY, &status, bios_total_bytes_global);
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

static void nvram_record_boot_success(unsigned char kind);

static void prepare_linux_platform(void) {
    bios_rtc_prepare_for_linux(nvram_enable_extended_cmos);
    acpi_install_for_linux();
}

static void fill_linux_loader_config(struct linux_loader_config* config) {
    config->total_bytes = bios_total_bytes_global;
    config->boot_priority = bios_boot_priority;
    config->vmlinux_partition = bios_linux_vmlinux_partition;
    config->enable_serial_console =
        (bios_nvram_flags0 & BIOS_NVRAM_FLAGS0_SERIAL_CONSOLE) != 0u ? 1u : 0u;
    config->enable_vesa_1024_768 =
        (bios_nvram_flags0 & BIOS_NVRAM_FLAGS0_VESA_1024_768) != 0u ? 1u : 0u;
    config->cmdline_suffix = bios_linux_cmdline_suffix;
    config->prepare_platform = prepare_linux_platform;
    config->init_vgabios = init_vgabios_for_linux;
    config->record_boot_success = nvram_record_boot_success;
    config->vbe_mode_info_buffer = legacy_vbe_mode_info_buffer;
    config->vbe_mode_info_pm32 = bios_call_vbe_mode_info_pm32;
    config->vbe_set_mode_pm32 = bios_call_vbe_set_mode_pm32;
}

static void install_bios_thunks(void) {
    struct legacy_runtime_config config;

    config.total_bytes = bios_total_bytes_global;
    config.floppy_present = legacy_floppy_present();
    config.hdd_present = bios_hdd_is_present();
    config.base_mem_kb = bios_dos_base_mem_kb;
    config.ebda_segment = bios_ebda_segment;
    config.boot_priority = bios_boot_priority;
    config.record_boot_success = nvram_record_boot_success;
    config.boot_pm32 = bios_boot_freedos_pm32;
    config.install_shadow = install_bios_shadow;
    legacy_runtime_init(&config);
}

#define BIOS_MEMTEST_START 0x00100000u
#define BIOS_MEMTEST_MARK_STEP 0x00100000u
static void prepare_boot_sector(void) {
    bios_boot_drive = legacy_prepare_boot_sector(
        bios_boot_priority, nvram_record_boot_success);
}

static void install_boot_drive(void) {
    legacy_install_boot_drive(bios_boot_drive);
}

static unsigned int bios_top_reserved_base(void) {
    return bios_memory_top_reserved_base(bios_total_bytes_global);
}

static unsigned int bios_pm_stack_top(void) {
    if (bios_total_bytes_global >= 0x00300000u) {
        return (bios_total_bytes_global & ~0xfffu) - 0x1000u;
    }
    return 0x001ff000u;
}

static void install_pm_stack_top(void) {
    legacy_install_pm_stack_top(bios_pm_stack_top());
}

static unsigned int bios_extended_usable_end(void) {
    return bios_memory_extended_usable_end(bios_total_bytes_global);
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
    if (bios_shared_service_global != 0 &&
        bios_shared_service_global->service_base != 0u &&
        addr >= bios_shared_service_global->service_base &&
        addr < bios_shared_service_global->service_base +
                   bios_shared_service_global->service_size) {
        return bios_shared_service_global->service_base +
               bios_shared_service_global->service_size;
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
    unsigned short cnt;

    if (bios_rsdp_linear_global == 0u) {
        serial_write_string("ACPI tables missing\r\n");
        return;
    }
    serial_write_string("ACPI RSDP=");
    serial_write_hex32(bios_rsdp_linear_global);
    serial_write_string("\r\n");
    acpi_clear_pm_events(bios_acpi_pm1_evt_global, bios_acpi_gpe0_global,
                         bios_acpi_gpe0_len_global);
    if ((bios_acpi_flags_global & SHARED_BOOT_ACPI_FLAG_ENABLE_SCI) != 0u &&
        bios_acpi_pm1_cnt_global != 0u) {
        cnt = inw((unsigned short)bios_acpi_pm1_cnt_global);
        if ((cnt & ACPI_PM1_CNT_SCI_EN) == 0u) {
            outw((unsigned short)bios_acpi_pm1_cnt_global,
                 (unsigned short)(cnt | ACPI_PM1_CNT_SCI_EN));
            cnt = inw((unsigned short)bios_acpi_pm1_cnt_global);
        }
        serial_write_string("ACPI PM1 cnt=");
        serial_write_hex16(cnt);
        serial_write_string("\r\n");
    }
}

static void run_test_elf_blob(void) {
    typedef unsigned int (*test_elf_entry_fn)(unsigned int, unsigned int,
                                             unsigned int, unsigned int);
    blob_expand_fn expand = bios_blob_expand_fn();
    struct blob_status status;
    struct linux_loader_config linux_config;
    unsigned char* image = (unsigned char*)LINUX_LOADER_TEST_ELF_IMAGE_LINEAR;
    unsigned int entry_phys = 0u;
    unsigned int rc;
    int expand_rc;

    if (bios_test_elf_blob_linear_global == 0u) {
        serial_write_string("No test ELF blob\r\n");
        return;
    }
    if (expand == 0) {
        serial_write_string("Test ELF blob service missing\r\n");
        return;
    }

    serial_write_string("Run ROM test ELF...\r\n");
    expand_rc = expand((const void*)bios_test_elf_blob_linear_global,
                       (void*)BLOB_STAGE_LINEAR, image,
                       LINUX_LOADER_TEST_ELF_IMAGE_CAPACITY, &status,
                       bios_total_bytes_global);
    if (expand_rc != 0) {
        serial_write_string("Test ELF blob failed rc=");
        serial_write_hex8((unsigned char)expand_rc);
        serial_write_string(" block=");
        serial_write_hex32(status.block);
        serial_write_string("\r\n");
        return;
    }

    fill_linux_loader_config(&linux_config);
    if (linux_loader_load_elf_image(&linux_config, image, status.output_size,
                                    &entry_phys) != 0) {
        return;
    }

    storage_scan(bios_total_bytes_global);
    install_bios_thunks();
    install_boot_drive();
    install_pm_stack_top();
    install_vgabios_shadow();
    linux_loader_prepare_boot_params(&linux_config, entry_phys, 0u, 0u);

    serial_write_string("Call test ELF entry=");
    serial_write_hex32(entry_phys);
    serial_write_string(" params=");
    serial_write_hex32(LINUX_LOADER_BOOT_PARAMS);
    serial_write_string("\r\n");
    cpu_serialize();
    rc = ((test_elf_entry_fn)entry_phys)(
        LINUX_LOADER_BOOT_PARAMS, LINUX_LOADER_RSDP_LINEAR,
        bios_acpi_pm1_evt_global, bios_acpi_pm1_cnt_global);
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

static int try_boot_linux(void) {
    struct linux_loader_config config;

    fill_linux_loader_config(&config);
    return linux_loader_try_boot(&config);
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

static unsigned int bios_payload_blob_ptr(unsigned int payload_id) {
    struct shared_payload_entry* payload =
        shared_payload_find(bios_shared_service_global, payload_id);

    if (payload == 0) {
        return 0u;
    }
    return payload->blob_ptr;
}

static void bios_load_stage_context(unsigned int total_bytes) {
    struct shared_boot_context* boot_ctx;
    unsigned int blob;

    bios_total_bytes_global = total_bytes;
    bios_shared_service_global = shared_service_from_total(total_bytes);
    bios_vgabios_blob_linear_global = 0u;
    bios_test_elf_blob_linear_global = 0u;
    bios_rsdp_linear_global = 0u;
    bios_acpi_pm1_evt_global = 0u;
    bios_acpi_pm1_cnt_global = 0u;
    bios_acpi_gpe0_global = 0u;
    bios_acpi_gpe0_len_global = 0u;
    bios_acpi_flags_global = 0u;
    bios_maintenance_requested = 0u;
    bios_shadow_ready = 0u;

    boot_ctx = shared_boot_context(bios_shared_service_global);
    if (boot_ctx != 0) {
        if ((boot_ctx->flags & SHARED_BOOT_FLAG_MAINTENANCE_REQUESTED) != 0u) {
            bios_maintenance_requested = 1u;
        }
        if ((boot_ctx->flags & SHARED_BOOT_FLAG_SHADOW_READY) != 0u) {
            bios_shadow_ready = 1u;
        }
        bios_rsdp_linear_global = boot_ctx->rsdp_linear;
        bios_acpi_pm1_evt_global = boot_ctx->acpi_pm1_evt;
        bios_acpi_pm1_cnt_global = boot_ctx->acpi_pm1_cnt;
        bios_acpi_gpe0_global = boot_ctx->acpi_gpe0;
        bios_acpi_gpe0_len_global = boot_ctx->acpi_gpe0_len;
        bios_acpi_flags_global = boot_ctx->acpi_flags;
    }

    blob = bios_payload_blob_ptr(SHARED_PAYLOAD_ID_VGABIOS);
    if (blob != 0u) {
        bios_vgabios_blob_linear_global = blob;
    }
    blob = bios_payload_blob_ptr(SHARED_PAYLOAD_ID_TEST_ELF);
    if (blob != 0u) {
        bios_test_elf_blob_linear_global = blob;
    }

}

void postcar_resume(unsigned int total_bytes, unsigned int aux_blob_linear) {
    volatile unsigned int stack_cookie = 0x13579bdfu;

    (void)aux_blob_linear;
    bios_load_stage_context(total_bytes);
    storage_set_scratch_base(bios_top_reserved_base());
    nvram_load_settings();
    legacy_floppy_probe();
    outb(0x80, POST_DRAM_STACK);
    serial_write_string("stage3 @ 00200000\r\n");
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
    serial_write_hex32(LEGACY_THUNK_RUNTIME_BASE);
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
