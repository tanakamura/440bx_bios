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
__attribute__((section(".qentry"))) void bios32_qemu_entry(
    unsigned int total_bytes, unsigned int aux_blob_linear);
extern void bios_boot_freedos_pm32(void);
extern void bios_call_vgabios_init_pm32(void);
extern unsigned int bios_call_vbe_mode_info_pm32(unsigned int mode);
extern unsigned int bios_call_vbe_set_mode_pm32(unsigned int mode);
extern unsigned char __bss_start[];
extern unsigned char __bss_end[];

static unsigned int bios_total_bytes_global = 0;
static unsigned int bios_dsdt_blob_linear_global = 0;
static unsigned int bios_vgabios_blob_linear_global = 0;
static unsigned int bios_test_elf_blob_linear_global = 0;
static struct shared_service_table* bios_shared_service_global = 0;
static unsigned char bios_vgabios_shadow_ready = 0;
static unsigned char bios_vgabios_initialized = 0;
static unsigned char bios_qemu_mode = 0;
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
    return (blob_expand_fn)BLOB_SERVICE_LINEAR;
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
#define BLOB_SERVICE_RESERVED_SIZE 0x00001000u

static void prepare_boot_sector(void) {
    bios_boot_drive = legacy_prepare_boot_sector(
        bios_boot_priority, nvram_record_boot_success);
}

static void install_boot_drive(void) {
    legacy_install_boot_drive(bios_boot_drive);
}

static void bios_memset(void* dst, unsigned char value, unsigned int len) {
    unsigned char* p = (unsigned char*)dst;
    while (len-- != 0u) {
        *p++ = value;
    }
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
    blob_expand_fn expand = bios_blob_expand_fn();
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

static void run_test_elf_blob(void) {
    typedef unsigned int (*test_elf_entry_fn)(unsigned int, unsigned int,
                                             unsigned int, unsigned int);
    blob_expand_fn expand = bios_blob_expand_fn();
    struct blob_status status;
    struct linux_loader_config linux_config;
    unsigned char* image = (unsigned char*)LINUX_LOADER_TEST_ELF_IMAGE_LINEAR;
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
                       LINUX_LOADER_TEST_ELF_IMAGE_CAPACITY, &status);
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
        LINUX_LOADER_BOOT_PARAMS, LINUX_LOADER_RSDP_LINEAR, pm1_evt, pm1_cnt);
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

static void bios_load_stage_context(unsigned int total_bytes,
                                    unsigned int aux_blob_linear,
                                    unsigned char qemu_mode) {
    struct shared_boot_context* boot_ctx;
    const unsigned int* aux = (const unsigned int*)aux_blob_linear;
    unsigned int blob;

    bios_total_bytes_global = total_bytes;
    bios_qemu_mode = qemu_mode;
    bios_shared_service_global = shared_service_from_total(total_bytes);
    bios_dsdt_blob_linear_global = 0u;
    bios_vgabios_blob_linear_global = 0u;
    bios_test_elf_blob_linear_global = 0u;
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
        if ((boot_ctx->flags & SHARED_BOOT_FLAG_PLATFORM_QEMU) != 0u) {
            bios_qemu_mode = 1u;
        }
        if ((boot_ctx->flags & SHARED_BOOT_FLAG_PLATFORM_P2B98_XV) != 0u) {
            bios_qemu_mode = 0u;
        }
    }

    blob = bios_payload_blob_ptr(SHARED_PAYLOAD_ID_DSDT);
    if (blob != 0u) {
        bios_dsdt_blob_linear_global = blob;
    }
    blob = bios_payload_blob_ptr(SHARED_PAYLOAD_ID_VGABIOS);
    if (blob != 0u) {
        bios_vgabios_blob_linear_global = blob;
    }
    blob = bios_payload_blob_ptr(SHARED_PAYLOAD_ID_TEST_ELF);
    if (blob != 0u) {
        bios_test_elf_blob_linear_global = blob;
    }

    if (aux_blob_linear != 0u) {
        if (bios_dsdt_blob_linear_global == 0u) {
            bios_dsdt_blob_linear_global = aux[BOOT_AUX_DSDT_BLOB];
        }
        if (bios_vgabios_blob_linear_global == 0u) {
            bios_vgabios_blob_linear_global = aux[BOOT_AUX_VBIOS_BLOB];
        }
        if (bios_test_elf_blob_linear_global == 0u) {
            bios_test_elf_blob_linear_global = aux[BOOT_AUX_TEST_ELF_BLOB];
        }
        if (aux[BOOT_AUX_MAINTENANCE] != 0u) {
            bios_maintenance_requested = 1u;
        }
        if (aux[BOOT_AUX_SHADOW_READY] != 0u) {
            bios_shadow_ready = 1u;
        }
    }
}

void postcar_resume(unsigned int total_bytes, unsigned int aux_blob_linear) {
    volatile unsigned int stack_cookie = 0x13579bdfu;

    bios_load_stage_context(total_bytes, aux_blob_linear, 0u);
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

void bios32_qemu_entry(unsigned int total_bytes, unsigned int aux_blob_linear) {
    volatile unsigned int stack_cookie = 0x2468ace0u;

    zero_bss();
    bios_load_stage_context(total_bytes, aux_blob_linear, 1u);
    storage_set_scratch_base(bios_top_reserved_base());
    nvram_load_settings();
    legacy_floppy_probe();
    serial_write_string("QEMU stage3 @ 00200000\r\n");
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
