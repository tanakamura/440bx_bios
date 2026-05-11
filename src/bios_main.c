#include "post_code.h"
#include "blob.h"

static inline void outb(unsigned short port, unsigned char value) {
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline unsigned char inb(unsigned short port) {
    unsigned char value;
    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline void outw(unsigned short port, unsigned short value) {
    __asm__ volatile("outw %0, %1" : : "a"(value), "Nd"(port));
}

static inline unsigned short inw(unsigned short port) {
    unsigned short value;
    __asm__ volatile("inw %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline void outl(unsigned short port, unsigned int value) {
    __asm__ volatile("outl %0, %1" : : "a"(value), "Nd"(port));
}

static inline unsigned int inl(unsigned short port) {
    unsigned int value;
    __asm__ volatile("inl %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

void bios32_entry_c(unsigned int total_bytes, unsigned int fdos_blob_linear);
__attribute__((section(".qentry"))) void bios32_qemu_entry(
    unsigned int total_bytes, unsigned int fdos_blob_linear);
extern unsigned char bios16_thunk_start[];
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
extern void bios_boot_freedos_pm32(void);
extern unsigned int bios16_pm_stack_top;
extern unsigned char bios16_thunk_end[];
extern unsigned char __bss_start[];
extern unsigned char __bss_end[];

static unsigned int bios_total_bytes_global = 0;
static const unsigned int bios16_thunk_runtime_base = 0x000f0000u;
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
static unsigned char bios_hdd_present = 0;

static void zero_bss(void) {
    unsigned char* p = __bss_start;
    while (p < __bss_end) {
        *p++ = 0u;
    }
}

static unsigned int pci_read32(unsigned char bus, unsigned char device,
                               unsigned char function, unsigned char reg) {
    unsigned int address = 0x80000000u | ((unsigned int)bus << 16) |
                           ((unsigned int)device << 11) |
                           ((unsigned int)function << 8) | (reg & 0xfcu);
    outl(0x0cf8, address);
    return inl(0x0cfc);
}

static void pci_write32(unsigned char bus, unsigned char device,
                        unsigned char function, unsigned char reg,
                        unsigned int val) {
    unsigned int address = 0x80000000u | ((unsigned int)bus << 16) |
                           ((unsigned int)device << 11) |
                           ((unsigned int)function << 8) | (reg & 0xfcu);
    outl(0x0cf8, address);
    outl(0x0cfc, val);
}

static unsigned short pci_read16(unsigned char bus, unsigned char device,
                                 unsigned char function, unsigned char reg) {
    unsigned int value = pci_read32(bus, device, function, reg);
    return (unsigned short)(value >> ((reg & 0x02u) * 8u));
}

static void pci_write16(unsigned char bus, unsigned char device,
                        unsigned char function, unsigned char reg,
                        unsigned short val) {
    unsigned char reg_lo = reg & 0x02u;
    unsigned int address = 0x80000000u | ((unsigned int)bus << 16) |
                           ((unsigned int)device << 11) |
                           ((unsigned int)function << 8) | (reg & 0xfcu);
    outl(0x0cf8, address);
    outw((unsigned short)(0x0cfc + reg_lo), val);
}

static void pci_write8(unsigned char bus, unsigned char device,
                       unsigned char function, unsigned char reg,
                       unsigned char val) {
    unsigned char reg_lo = reg & 0x03u;
    unsigned int address = 0x80000000u | ((unsigned int)bus << 16) |
                           ((unsigned int)device << 11) |
                           ((unsigned int)function << 8) | (reg & 0xfcu);
    outl(0x0cf8, address);
    outb((unsigned short)(0x0cfc + reg_lo), val);
}

static unsigned char pci_read8(unsigned char bus, unsigned char device,
                               unsigned char function, unsigned char reg) {
    unsigned int value = pci_read32(bus, device, function, reg);
    return (unsigned char)(value >> ((reg & 0x03u) * 8u));
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
#define IA32_MTRR_DEF_TYPE 0x2ffu
#define MTRR_DEF_TYPE_E 0x00000800u

#define BDA_TICK_COUNT 0x046cu
#define BDA_MIDNIGHT_FLAG 0x0470u

static unsigned char cmos_read(unsigned char index) {
    outb(0x0070u, (unsigned char)(index | 0x80u));
    return inb(0x0071u);
}

static void serial_write_string(const char* s);
static void serial_write_hex8(unsigned char value);

static void rtc_dump_raw(const char* tag) {
    serial_write_string("RTC ");
    serial_write_string(tag);
    serial_write_string(" A=");
    serial_write_hex8(cmos_read(0x0au));
    serial_write_string(" B=");
    serial_write_hex8(cmos_read(0x0bu));
    serial_write_string(" S=");
    serial_write_hex8(cmos_read(0x00u));
    serial_write_string(" M=");
    serial_write_hex8(cmos_read(0x02u));
    serial_write_string(" H=");
    serial_write_hex8(cmos_read(0x04u));
    serial_write_string(" D=");
    serial_write_hex8(cmos_read(0x07u));
    serial_write_string(" N=");
    serial_write_hex8(cmos_read(0x08u));
    serial_write_string(" Y=");
    serial_write_hex8(cmos_read(0x09u));
    serial_write_string("\r\n");
}

static void cmos_write(unsigned char index, unsigned char value) {
    outb(0x0070u, (unsigned char)(index | 0x80u));
    outb(0x0071u, value);
}

static unsigned char rtc_force_sane_mode(void) {
    unsigned char status_b = cmos_read(0x0bu);
    unsigned char sane_status_b = (unsigned char)((status_b & 0x79u) | 0x02u);
    if (sane_status_b != status_b) {
        cmos_write(0x0bu, sane_status_b);
    }
    return sane_status_b;
}

static unsigned char bin_to_bcd(unsigned char value) {
    return (unsigned char)(((value / 10u) << 4) | (value % 10u));
}

static unsigned char bcd_to_bin(unsigned char value) {
    return (unsigned char)(((value >> 4) * 10u) + (value & 0x0fu));
}

static unsigned char rtc_begin_set(void) {
    unsigned char sane_status_b = rtc_force_sane_mode();
    cmos_write(0x0bu, (unsigned char)(sane_status_b | 0x80u));
    return sane_status_b;
}

static void rtc_end_set(unsigned char sane_status_b) {
    cmos_write(0x0bu, sane_status_b);
}

static int rtc_wait_ready(void) {
    unsigned int timeout = 100000u;
    while (timeout-- != 0u) {
        if ((cmos_read(0x0au) & 0x80u) == 0) {
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
    sec = cmos_read(0x00u);
    min = cmos_read(0x02u);
    hour = cmos_read(0x04u);

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
    day = cmos_read(0x07u);
    mon = cmos_read(0x08u);
    year = cmos_read(0x09u);

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
    cmos_write(0x00u, sec_bcd);
    cmos_write(0x02u, min_bcd);
    cmos_write(0x04u, (unsigned char)(hour_bcd & 0x7fu));
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
    cmos_write(0x07u, day_bcd);
    cmos_write(0x08u, mon_bcd);
    cmos_write(0x09u, year_bcd);
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

static void bios_init_pit(void) {
    outb(0x0043u, 0x36u);
    outb(0x0040u, 0x00u);
    outb(0x0040u, 0x00u);
    bios_tick_initialized = 0;
    bios_tick_subcount = 0;
    bios_set_tick_counter(0u);
    *(volatile unsigned char*)BDA_MIDNIGHT_FLAG = 0u;
}

static void bios_update_tick_counter(void) {
    unsigned short raw = pit_read_counter0();
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
#define BDA_CURSOR_POS 0x0450u
#define BDA_CURSOR_SHAPE 0x0460u
#define BDA_ACTIVE_PAGE 0x0462u

static void serial_write_char(char c) {
    while ((inb(0x03f8 + 5) & 0x20) == 0) {
    }
    outb(0x03f8, (unsigned char)c);
}

static void serial_write_string(const char* s) {
    while (*s != '\0') {
        serial_write_char(*s);
        ++s;
    }
}

static void serial_write_hex4(unsigned char value) {
    value &= 0x0f;
    serial_write_char(
        (char)(value < 10 ? ('0' + value) : ('a' + (value - 10))));
}

static void serial_write_hex8(unsigned char value) {
    serial_write_hex4((unsigned char)(value >> 4));
    serial_write_hex4(value);
}

static void serial_write_hex16(unsigned short value) {
    serial_write_hex8((unsigned char)(value >> 8));
    serial_write_hex8((unsigned char)value);
}

static void serial_write_hex32(unsigned int value) {
    serial_write_hex16((unsigned short)(value >> 16));
    serial_write_hex16((unsigned short)value);
}

static void serial_dump_bytes(const char* tag, const unsigned char* data,
                              unsigned int len) {
    unsigned int i;
    serial_write_string(tag);
    serial_write_string(" @ ");
    serial_write_hex32((unsigned int)data);
    serial_write_string(":");
    for (i = 0; i < len; ++i) {
        serial_write_char(' ');
        serial_write_hex8(data[i]);
    }
    serial_write_string("\r\n");
}

static void serial_write_u32(unsigned int value) {
    char buf[10];
    unsigned int i = 0;

    if (value == 0) {
        serial_write_char('0');
        return;
    }

    while (value != 0 && i < sizeof(buf)) {
        buf[i++] = (char)('0' + (value % 10u));
        value /= 10u;
    }
    while (i != 0) {
        serial_write_char(buf[--i]);
    }
}

static void serial_write_fixed2(unsigned int x100) {
    serial_write_u32(x100 / 100u);
    serial_write_char('.');
    serial_write_char((char)('0' + ((x100 / 10u) % 10u)));
    serial_write_char((char)('0' + (x100 % 10u)));
}

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
    *(volatile unsigned short*)BDA_CURSOR_SHAPE = 0x0607u;
    *(volatile unsigned char*)BDA_ACTIVE_PAGE = 0x00u;
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

static const unsigned long long bios_gdt_template[] = {
    0x0000000000000000ull, 0x00cf9b000000ffffull, 0x00cf93000000ffffull,
    0x00009b100000ffffull, 0x000093100000ffffull,
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
    __asm__ volatile("movw $0x10, %%ax\n\t"
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

static void cache_disable_for_mtrr_update(void) {
    __asm__ volatile("movl %%cr0, %%eax\n\t"
                     "orl $0x40000000, %%eax\n\t"
                     "andl $0xdfffffff, %%eax\n\t"
                     "movl %%eax, %%cr0\n\t"
                     "wbinvd"
                     :
                     :
                     : "eax", "memory");
}

static void cache_enable_after_mtrr_update(void) {
    __asm__ volatile("wbinvd\n\t"
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
    volatile unsigned int* end = (volatile unsigned int*)0x00100000u;

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

static void install_bios_shadow(void) {
    load_bios_gdt(bios_gdt_template);
    enable_shadow_dram();
    clear_shadow_window();
    enable_shadow_wb_mtrrs();
    install_runtime_gdt();
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
    bios_floppy_dpt_linear = dpt_linear;
    install_ivt_vector(0x1e, dpt_linear);
    install_thunk_vector(0x40,
                         bios16_thunk_runtime_base +
                             (unsigned int)(bios16_int13 - bios16_thunk_start));
    for (i = 0; i < sizeof(floppy_dpt); ++i) {
        dpt[i] = floppy_dpt[i];
    }
    serialize_instruction_stream();
    *(volatile unsigned short*)0x0410u = 0x0001u;
    *(volatile unsigned short*)0x0413u = bios_dos_base_mem_kb;
    *(volatile unsigned short*)0x040eu = bios_ebda_segment;
    bios_kbd_init();
    bios_video_init();
    *(volatile unsigned char*)0x043eu = 0x01u;
    *(volatile unsigned char*)0x043fu = 0x00u;
    *(volatile unsigned char*)0x0440u = 0x25u;
    *(volatile unsigned char*)0x0441u = 0x00u;
    *(volatile unsigned char*)0x0474u = 0x00u;
    *(volatile unsigned char*)0x0475u = bios_hdd_present ? 1u : 0u;
    *(volatile unsigned char*)0x048bu = 0x00u;
    *(volatile unsigned char*)0x048cu = 0x00u;
    *(volatile unsigned char*)0x048du = 0x00u;
    *(volatile unsigned char*)0x048eu = 0x00u;
    *(volatile unsigned char*)0x048fu = 0x07u;
    *(volatile unsigned char*)0x0490u = 0x17u;
    *(volatile unsigned char*)0x0491u = 0x00u;
    *(volatile unsigned char*)0x0492u = 0x00u;
    bios_init_pit();
}

#define STORAGE_SECTOR_LINEAR 0x00500000u
#define STORAGE_ID_LINEAR 0x00501000u
#define USB_FRAME_LIST_LINEAR 0x00600000u
#define USB_QH_LINEAR 0x00601000u
#define USB_TD_LINEAR 0x00602000u
#define USB_BUF_LINEAR 0x00610000u
#define USB_CFG_BUF_LINEAR 0x00610400u
#define USB_CBW_BUF_LINEAR 0x00610800u
#define USB_CSW_BUF_LINEAR 0x00610900u
#define USB_SECTOR_LINEAR 0x00612000u
#define USB_MAX_TD 64u

struct storage_pci_bdf {
    unsigned char bus;
    unsigned char dev;
    unsigned char fn;
};

static struct storage_pci_bdf storage_ide_bdf;
static struct storage_pci_bdf storage_uhci_bdf;
static unsigned char storage_ide_found = 0;
static unsigned char storage_uhci_found = 0;
static unsigned int pci_next_io = 0xd000u;
static unsigned int pci_next_mem = 0xf0000000u;
static unsigned char usb_msd_quiet_status = 0;
static unsigned short bios_hdd_io = 0;
static unsigned short bios_hdd_ctrl = 0;
static unsigned char bios_hdd_drive = 0;
static unsigned int bios_hdd_total_sectors = 0;
static unsigned short bios_hdd_heads = 16;
static unsigned short bios_hdd_spt = 63;
static unsigned short bios_hdd_cylinders = 1;

static void storage_memset(void* dst, unsigned char value, unsigned int len) {
    unsigned char* p = (unsigned char*)dst;
    while (len-- != 0u) {
        *p++ = value;
    }
}

static unsigned short le16(const unsigned char* p) {
    return (unsigned short)((unsigned short)p[0] | ((unsigned short)p[1] << 8));
}

static unsigned int le32(const unsigned char* p) {
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8) |
           ((unsigned int)p[2] << 16) | ((unsigned int)p[3] << 24);
}

static void put16le(unsigned char* p, unsigned short value) {
    p[0] = (unsigned char)value;
    p[1] = (unsigned char)(value >> 8);
}

static void put32le(unsigned char* p, unsigned int value) {
    p[0] = (unsigned char)value;
    p[1] = (unsigned char)(value >> 8);
    p[2] = (unsigned char)(value >> 16);
    p[3] = (unsigned char)(value >> 24);
}

static void delay_approx_ms(unsigned int ms) {
    unsigned int i;
    while (ms-- != 0u) {
        for (i = 0; i < 4096u; ++i) {
            (void)inb(0x0080u);
        }
    }
}

static unsigned int align_up_u32(unsigned int value, unsigned int align) {
    if (align == 0u) {
        return value;
    }
    return (value + align - 1u) & ~(align - 1u);
}

static void pci_print_bdf(unsigned char bus, unsigned char dev,
                          unsigned char fn) {
    serial_write_hex8(bus);
    serial_write_char(':');
    serial_write_hex8(dev);
    serial_write_char('.');
    serial_write_hex8(fn);
}

static void pci_assign_resources(unsigned char bus, unsigned char dev,
                                 unsigned char fn, unsigned int class_code,
                                 unsigned char header_type) {
    unsigned char bar_index;
    unsigned short command = pci_read16(bus, dev, fn, 0x04u);
    unsigned short new_command = command;

    if ((header_type & 0x7fu) != 0x00u) {
        return;
    }

    for (bar_index = 0; bar_index < 6u; ++bar_index) {
        unsigned char reg = (unsigned char)(0x10u + bar_index * 4u);
        unsigned int orig = pci_read32(bus, dev, fn, reg);
        unsigned int mask;
        unsigned int size;
        unsigned int base;

        if (orig == 0xffffffffu) {
            continue;
        }

        pci_write32(bus, dev, fn, reg, 0xffffffffu);
        mask = pci_read32(bus, dev, fn, reg);
        pci_write32(bus, dev, fn, reg, orig);

        if (mask == 0u || mask == 0xffffffffu) {
            continue;
        }

        if ((orig & 0x00000001u) != 0u) {
            size = (~(mask & 0xfffffffcu)) + 1u;
            base = orig & 0xfffffffcu;
            if (size == 0u || size > 0x1000u) {
                continue;
            }
            if (base == 0u) {
                base = align_up_u32(pci_next_io, size);
                pci_next_io = base + size;
                pci_write32(bus, dev, fn, reg, base | 0x00000001u);
                serial_write_string("  assign BAR");
                serial_write_hex8(bar_index);
                serial_write_string(" io=");
                serial_write_hex16((unsigned short)base);
                serial_write_string(" sz=");
                serial_write_hex16((unsigned short)size);
                serial_write_string("\r\n");
            }
            new_command |= 0x0001u;
        } else {
            unsigned int bar_type = orig & 0x00000006u;
            size = (~(mask & 0xfffffff0u)) + 1u;
            base = orig & 0xfffffff0u;
            if (size == 0u || size > 0x01000000u) {
                continue;
            }
            if (base == 0u) {
                base = align_up_u32(pci_next_mem, size);
                pci_next_mem = base + size;
                pci_write32(bus, dev, fn, reg, base | (orig & 0x0000000fu));
                serial_write_string("  assign BAR");
                serial_write_hex8(bar_index);
                serial_write_string(" mem=");
                serial_write_hex32(base);
                serial_write_string(" sz=");
                serial_write_hex32(size);
                serial_write_string("\r\n");
            }
            new_command |= 0x0002u;
            if (bar_type == 0x00000004u) {
                ++bar_index;
            }
        }
    }

    if (((class_code >> 16) == 0x01u) || ((class_code >> 16) == 0x0cu)) {
        new_command |= 0x0004u;
    }
    if (new_command != command) {
        pci_write16(bus, dev, fn, 0x04u, new_command);
        serial_write_string("  cmd ");
        serial_write_hex16(command);
        serial_write_string("->");
        serial_write_hex16(new_command);
        serial_write_string("\r\n");
    }
}

static void pci_enumerate_and_assign(void) {
    unsigned char bus;
    unsigned char dev;
    unsigned char fn;

    outb(0x80, POST_PCI_PROBE_START);
    storage_ide_found = 0;
    storage_uhci_found = 0;
    pci_next_io = 0xd000u;
    pci_next_mem = 0xf0000000u;

    serial_write_string("PCI enum...\r\n");
    for (bus = 0; bus < 4u; ++bus) {
        for (dev = 0; dev < 32u; ++dev) {
            unsigned short vendor0 = pci_read16(bus, dev, 0, 0x00u);
            unsigned char header0;
            unsigned char fn_count;

            if (vendor0 == 0xffffu) {
                continue;
            }
            header0 = pci_read8(bus, dev, 0, 0x0eu);
            fn_count = (header0 & 0x80u) != 0u ? 8u : 1u;
            for (fn = 0; fn < fn_count; ++fn) {
                unsigned int id = pci_read32(bus, dev, fn, 0x00u);
                unsigned int revclass;
                unsigned int class_code;
                unsigned char header;
                unsigned short vendor = (unsigned short)id;
                unsigned short device_id = (unsigned short)(id >> 16);
                unsigned short command;

                if (vendor == 0xffffu) {
                    continue;
                }
                revclass = pci_read32(bus, dev, fn, 0x08u);
                class_code = revclass >> 8;
                header = pci_read8(bus, dev, fn, 0x0eu);
                command = pci_read16(bus, dev, fn, 0x04u);

                serial_write_string("PCI ");
                pci_print_bdf(bus, dev, fn);
                serial_write_char(' ');
                serial_write_hex16(vendor);
                serial_write_char(':');
                serial_write_hex16(device_id);
                serial_write_string(" cls=");
                serial_write_hex32(class_code);
                serial_write_string(" cmd=");
                serial_write_hex16(command);
                serial_write_string("\r\n");

                pci_assign_resources(bus, dev, fn, class_code, header);

                if (((class_code >> 16) & 0xffu) == 0x01u &&
                    ((class_code >> 8) & 0xffu) == 0x01u) {
                    storage_ide_bdf.bus = bus;
                    storage_ide_bdf.dev = dev;
                    storage_ide_bdf.fn = fn;
                    storage_ide_found = 1u;
                }
                if (class_code == 0x0c0300u) {
                    storage_uhci_bdf.bus = bus;
                    storage_uhci_bdf.dev = dev;
                    storage_uhci_bdf.fn = fn;
                    storage_uhci_found = 1u;
                }
            }
        }
    }
}

#define IDE_STATUS_BSY 0x80u
#define IDE_STATUS_DRDY 0x40u
#define IDE_STATUS_DRQ 0x08u
#define IDE_STATUS_ERR 0x01u

static void ide_400ns_delay(unsigned short ctrl) {
    (void)inb(ctrl);
    (void)inb(ctrl);
    (void)inb(ctrl);
    (void)inb(ctrl);
}

static int ide_wait_not_busy(unsigned short io) {
    unsigned int timeout = 2000000u;
    unsigned char st;
    do {
        st = inb((unsigned short)(io + 7u));
        if ((st & IDE_STATUS_BSY) == 0u) {
            return 0;
        }
    } while (--timeout != 0u);
    return -1;
}

static int ide_wait_drq(unsigned short io) {
    unsigned int timeout = 2000000u;
    unsigned char st;
    do {
        st = inb((unsigned short)(io + 7u));
        if ((st & IDE_STATUS_ERR) != 0u) {
            return -1;
        }
        if ((st & IDE_STATUS_BSY) == 0u && (st & IDE_STATUS_DRQ) != 0u) {
            return 0;
        }
    } while (--timeout != 0u);
    return -1;
}

static int ide_identify(unsigned short io, unsigned short ctrl,
                        unsigned char drive, unsigned short* words) {
    unsigned int i;
    unsigned char st;

    outb(ctrl, 0x02u);
    outb((unsigned short)(io + 6u), (unsigned char)(0xa0u | (drive << 4)));
    ide_400ns_delay(ctrl);
    if (ide_wait_not_busy(io) != 0) {
        return -1;
    }
    outb((unsigned short)(io + 2u), 0u);
    outb((unsigned short)(io + 3u), 0u);
    outb((unsigned short)(io + 4u), 0u);
    outb((unsigned short)(io + 5u), 0u);
    outb((unsigned short)(io + 7u), 0xecu);
    st = inb((unsigned short)(io + 7u));
    if (st == 0u || st == 0xffu) {
        return -1;
    }
    if (ide_wait_not_busy(io) != 0) {
        return -1;
    }
    if (inb((unsigned short)(io + 4u)) != 0u ||
        inb((unsigned short)(io + 5u)) != 0u) {
        return -1;
    }
    if (ide_wait_drq(io) != 0) {
        return -1;
    }
    for (i = 0; i < 256u; ++i) {
        words[i] = inw(io);
    }
    return 0;
}

static int ide_read_lba28(unsigned short io, unsigned short ctrl,
                          unsigned char drive, unsigned int lba,
                          unsigned char* dst) {
    unsigned int i;

    outb(ctrl, 0x02u);
    outb((unsigned short)(io + 6u),
         (unsigned char)(0xe0u | (drive << 4) | ((lba >> 24) & 0x0fu)));
    ide_400ns_delay(ctrl);
    if (ide_wait_not_busy(io) != 0) {
        return -1;
    }
    outb((unsigned short)(io + 2u), 1u);
    outb((unsigned short)(io + 3u), (unsigned char)lba);
    outb((unsigned short)(io + 4u), (unsigned char)(lba >> 8));
    outb((unsigned short)(io + 5u), (unsigned char)(lba >> 16));
    outb((unsigned short)(io + 7u), 0x20u);
    if (ide_wait_drq(io) != 0) {
        return -1;
    }
    for (i = 0; i < 256u; ++i) {
        ((unsigned short*)dst)[i] = inw(io);
    }
    ide_400ns_delay(ctrl);
    return 0;
}

static void bios_hdd_set_geometry(unsigned int sectors) {
    unsigned int cylinders;
    bios_hdd_spt = 63u;
    bios_hdd_heads = (sectors > (1024u * 16u * 63u)) ? 255u : 16u;
    cylinders = sectors / ((unsigned int)bios_hdd_heads * bios_hdd_spt);
    if (cylinders == 0u) {
        cylinders = 1u;
    }
    if (cylinders > 1024u) {
        cylinders = 1024u;
    }
    bios_hdd_cylinders = (unsigned short)cylinders;
}

static void bios_hdd_register(unsigned short io, unsigned short ctrl,
                              unsigned char drive, unsigned int sectors) {
    if (bios_hdd_present) {
        return;
    }
    bios_hdd_present = 1u;
    bios_hdd_io = io;
    bios_hdd_ctrl = ctrl;
    bios_hdd_drive = drive;
    bios_hdd_total_sectors = sectors;
    bios_hdd_set_geometry(sectors);
    serial_write_string("BIOS HDD80 sectors=");
    serial_write_hex32(sectors);
    serial_write_string(" C/H/S=");
    serial_write_u32(bios_hdd_cylinders);
    serial_write_char('/');
    serial_write_u32(bios_hdd_heads);
    serial_write_char('/');
    serial_write_u32(bios_hdd_spt);
    serial_write_string("\r\n");
}

static void ide_dump_partition_summary(const unsigned char* sector) {
    unsigned int i;

    serial_write_string("IDE MBR sig=");
    serial_write_hex8(sector[0x01feu]);
    serial_write_hex8(sector[0x01ffu]);
    serial_write_string("\r\n");
    for (i = 0; i < 4u; ++i) {
        const unsigned char* entry = sector + 0x01beu + i * 16u;
        serial_write_string("IDE part");
        serial_write_u32(i);
        serial_write_string(" boot=");
        serial_write_hex8(entry[0]);
        serial_write_string(" type=");
        serial_write_hex8(entry[4]);
        serial_write_string(" start=");
        serial_write_hex32(le32(entry + 8u));
        serial_write_string(" size=");
        serial_write_hex32(le32(entry + 12u));
        serial_write_string("\r\n");
    }
}

static void ide_scan_channel(const char* name, unsigned short io,
                             unsigned short ctrl) {
    unsigned char drive;
    unsigned short* id = (unsigned short*)STORAGE_ID_LINEAR;
    unsigned char* sector = (unsigned char*)STORAGE_SECTOR_LINEAR;

    for (drive = 0; drive < 2u; ++drive) {
        unsigned int sectors;
        serial_write_string("IDE ");
        serial_write_string(name);
        serial_write_char(drive == 0u ? 'M' : 'S');
        serial_write_string(" st=");
        serial_write_hex8(inb((unsigned short)(io + 7u)));
        serial_write_string(" alt=");
        serial_write_hex8(inb(ctrl));
        serial_write_string(" identify...");
        if (ide_identify(io, ctrl, drive, id) != 0) {
            serial_write_string(" none\r\n");
            continue;
        }
        sectors = ((unsigned int)id[61] << 16) | id[60];
        serial_write_string(" ok lba28=");
        serial_write_hex32(sectors);
        serial_write_string("\r\n");
        if (sectors != 0u) {
            bios_hdd_register(io, ctrl, drive, sectors);
        }
        if (ide_read_lba28(io, ctrl, drive, 0u, sector) == 0) {
            serial_dump_bytes("IDE LBA0", sector, 16u);
            if (drive == 0u && name[0] == 'p') {
                ide_dump_partition_summary(sector);
            }
        } else {
            serial_write_string("IDE LBA0 read failed\r\n");
        }
    }
}

static void ide_enable_piix4_legacy(void) {
    unsigned short cmd;
    unsigned short bmiba;
    unsigned short primary_timing;
    unsigned short secondary_timing;

    cmd = pci_read16(storage_ide_bdf.bus, storage_ide_bdf.dev,
                     storage_ide_bdf.fn, 0x04u);
    pci_write16(storage_ide_bdf.bus, storage_ide_bdf.dev, storage_ide_bdf.fn,
                0x04u, (unsigned short)(cmd | 0x0005u));

    primary_timing = pci_read16(storage_ide_bdf.bus, storage_ide_bdf.dev,
                                storage_ide_bdf.fn, 0x40u);
    secondary_timing = pci_read16(storage_ide_bdf.bus, storage_ide_bdf.dev,
                                  storage_ide_bdf.fn, 0x42u);
    pci_write16(storage_ide_bdf.bus, storage_ide_bdf.dev, storage_ide_bdf.fn,
                0x40u, (unsigned short)(primary_timing | 0x8000u));
    pci_write16(storage_ide_bdf.bus, storage_ide_bdf.dev, storage_ide_bdf.fn,
                0x42u, (unsigned short)(secondary_timing | 0x8000u));

    bmiba = pci_read16(storage_ide_bdf.bus, storage_ide_bdf.dev,
                       storage_ide_bdf.fn, 0x20u);
    serial_write_string("IDE cfg cmd=");
    serial_write_hex16(pci_read16(storage_ide_bdf.bus, storage_ide_bdf.dev,
                                  storage_ide_bdf.fn, 0x04u));
    serial_write_string(" bmiba=");
    serial_write_hex16(bmiba);
    serial_write_string(" pri=");
    serial_write_hex16(pci_read16(storage_ide_bdf.bus, storage_ide_bdf.dev,
                                  storage_ide_bdf.fn, 0x40u));
    serial_write_string(" sec=");
    serial_write_hex16(pci_read16(storage_ide_bdf.bus, storage_ide_bdf.dev,
                                  storage_ide_bdf.fn, 0x42u));
    serial_write_string("\r\n");
}

static void ide_scan(void) {
    if (!storage_ide_found) {
        serial_write_string("IDE: controller not found\r\n");
        return;
    }
    ide_enable_piix4_legacy();
    serial_write_string("IDE scan ");
    pci_print_bdf(storage_ide_bdf.bus, storage_ide_bdf.dev, storage_ide_bdf.fn);
    serial_write_string("\r\n");
    ide_scan_channel("pri", 0x01f0u, 0x03f6u);
    ide_scan_channel("sec", 0x0170u, 0x0376u);
}

struct uhci_td {
    volatile unsigned int link;
    volatile unsigned int status;
    volatile unsigned int token;
    volatile unsigned int buffer;
    volatile unsigned int sw[4];
};

struct uhci_qh {
    volatile unsigned int head;
    volatile unsigned int element;
};

struct usb_dev {
    unsigned short io;
    unsigned char addr;
    unsigned char low_speed;
    unsigned char ep0_mps;
    unsigned char bulk_in;
    unsigned char bulk_out;
    unsigned short bulk_in_mps;
    unsigned short bulk_out_mps;
    unsigned char bulk_in_toggle;
    unsigned char bulk_out_toggle;
    unsigned char interface_number;
};

#define UHCI_USBCMD 0x00u
#define UHCI_USBSTS 0x02u
#define UHCI_USBINTR 0x04u
#define UHCI_FRNUM 0x06u
#define UHCI_FLBASEADD 0x08u
#define UHCI_SOFMOD 0x0cu
#define UHCI_PORTSC1 0x10u

#define UHCI_CMD_RS 0x0001u
#define UHCI_CMD_HCRESET 0x0002u
#define UHCI_CMD_CF 0x0040u
#define UHCI_CMD_MAXP 0x0080u

#define UHCI_PORT_CCS 0x0001u
#define UHCI_PORT_CSC 0x0002u
#define UHCI_PORT_PE 0x0004u
#define UHCI_PORT_PEC 0x0008u
#define UHCI_PORT_LSDA 0x0100u
#define UHCI_PORT_PR 0x0200u
#define UHCI_PORT_CHANGE (UHCI_PORT_CSC | UHCI_PORT_PEC)

#define UHCI_PTR_TERM 0x00000001u
#define UHCI_PTR_QH 0x00000002u
#define UHCI_PTR_DEPTH 0x00000004u

#define UHCI_TD_ACTIVE 0x00800000u
#define UHCI_TD_STALLED 0x00400000u
#define UHCI_TD_DBE 0x00200000u
#define UHCI_TD_BABBLE 0x00100000u
#define UHCI_TD_NAK 0x00080000u
#define UHCI_TD_CRC_TIMEOUT 0x00040000u
#define UHCI_TD_BITSTUFF 0x00020000u
#define UHCI_TD_ERR3 0x18000000u
#define UHCI_TD_LOW_SPEED 0x04000000u

#define USB_PID_OUT 0xe1u
#define USB_PID_IN 0x69u
#define USB_PID_SETUP 0x2du

static struct uhci_td* uhci_td_base(void) {
    return (struct uhci_td*)USB_TD_LINEAR;
}

static struct uhci_qh* uhci_qh(void) { return (struct uhci_qh*)USB_QH_LINEAR; }

static unsigned int uhci_td_phys(unsigned int index) {
    return USB_TD_LINEAR + index * sizeof(struct uhci_td);
}

static unsigned int uhci_token(unsigned char pid, unsigned char addr,
                               unsigned char ep, unsigned char toggle,
                               unsigned int len) {
    unsigned int max_len = (len == 0u) ? 0x7ffu : (len - 1u);
    return (unsigned int)pid | ((unsigned int)addr << 8) |
           ((unsigned int)(ep & 0x0fu) << 15) |
           ((unsigned int)(toggle & 1u) << 19) | (max_len << 21);
}

static void uhci_prepare_schedule(unsigned short io) {
    volatile unsigned int* frame = (volatile unsigned int*)USB_FRAME_LIST_LINEAR;
    struct uhci_qh* qh = uhci_qh();
    unsigned int i;

    for (i = 0; i < 1024u; ++i) {
        frame[i] = USB_QH_LINEAR | UHCI_PTR_QH;
    }
    qh->head = UHCI_PTR_TERM;
    qh->element = UHCI_PTR_TERM;
    storage_memset((void*)USB_TD_LINEAR, 0u,
                   USB_MAX_TD * sizeof(struct uhci_td));

    outw((unsigned short)(io + UHCI_USBCMD), 0u);
    outw((unsigned short)(io + UHCI_USBSTS), 0x003fu);
    outw((unsigned short)(io + UHCI_USBINTR), 0u);
    outw((unsigned short)(io + UHCI_FRNUM), 0u);
    outl((unsigned short)(io + UHCI_FLBASEADD), USB_FRAME_LIST_LINEAR);
    outb((unsigned short)(io + UHCI_SOFMOD), 0x40u);
}

static void uhci_td_setup(unsigned int index, unsigned int link,
                          unsigned int status, unsigned int token,
                          unsigned int buffer) {
    struct uhci_td* td = uhci_td_base() + index;
    td->link = link;
    td->status = status;
    td->token = token;
    td->buffer = buffer;
    td->sw[0] = td->sw[1] = td->sw[2] = td->sw[3] = 0u;
}

static int uhci_run_chain(unsigned short io, unsigned int td_count) {
    struct uhci_td* td = uhci_td_base();
    struct uhci_qh* qh = uhci_qh();
    unsigned int timeout = 4000000u;
    unsigned int i;

    if (td_count == 0u || td_count > USB_MAX_TD) {
        return -1;
    }

    qh->head = UHCI_PTR_TERM;
    qh->element = uhci_td_phys(0);
    cache_writeback_invalidate();

    outw((unsigned short)(io + UHCI_USBSTS), 0x003fu);
    outw((unsigned short)(io + UHCI_USBCMD),
         UHCI_CMD_CF | UHCI_CMD_MAXP | UHCI_CMD_RS);

    while (timeout-- != 0u) {
        if ((td[td_count - 1u].status & UHCI_TD_ACTIVE) == 0u) {
            break;
        }
        if ((timeout & 0x3ffu) == 0u) {
            (void)inw((unsigned short)(io + UHCI_USBSTS));
        }
    }

    outw((unsigned short)(io + UHCI_USBCMD), UHCI_CMD_CF | UHCI_CMD_MAXP);
    cache_writeback_invalidate();

    for (i = 0; i < td_count; ++i) {
        unsigned int st = td[i].status;
        if ((st & (UHCI_TD_ACTIVE | UHCI_TD_STALLED | UHCI_TD_DBE |
                   UHCI_TD_BABBLE | UHCI_TD_CRC_TIMEOUT |
                   UHCI_TD_BITSTUFF)) != 0u) {
            serial_write_string("UHCI td");
            serial_write_u32(i);
            serial_write_string(" st=");
            serial_write_hex32(st);
            serial_write_string(" tok=");
            serial_write_hex32(td[i].token);
            serial_write_string(" us=");
            serial_write_hex16(inw((unsigned short)(io + UHCI_USBSTS)));
            serial_write_string("\r\n");
            return -1;
        }
    }
    return 0;
}

static int uhci_control(struct usb_dev* dev, unsigned char req_type,
                        unsigned char req, unsigned short value,
                        unsigned short index, void* data, unsigned int len,
                        unsigned char dir_in) {
    unsigned char* setup = (unsigned char*)USB_BUF_LINEAR;
    unsigned int td_count = 0;
    unsigned int offset = 0;
    unsigned char toggle = 1u;
    unsigned int status = UHCI_TD_ACTIVE | UHCI_TD_ERR3 |
                          (dev->low_speed ? UHCI_TD_LOW_SPEED : 0u);

    setup[0] = req_type;
    setup[1] = req;
    put16le(setup + 2, value);
    put16le(setup + 4, index);
    put16le(setup + 6, (unsigned short)len);

    uhci_td_setup(td_count,
                  (len == 0u) ? (uhci_td_phys(1u) | UHCI_PTR_DEPTH)
                               : (uhci_td_phys(1u) | UHCI_PTR_DEPTH),
                  status, uhci_token(USB_PID_SETUP, dev->addr, 0u, 0u, 8u),
                  USB_BUF_LINEAR);
    ++td_count;

    while (offset < len) {
        unsigned int chunk = len - offset;
        unsigned char pid = dir_in ? USB_PID_IN : USB_PID_OUT;
        if (chunk > dev->ep0_mps) {
            chunk = dev->ep0_mps;
        }
        if (td_count + 1u >= USB_MAX_TD) {
            return -1;
        }
        uhci_td_setup(td_count, uhci_td_phys(td_count + 1u) | UHCI_PTR_DEPTH,
                      status,
                      uhci_token(pid, dev->addr, 0u, toggle, chunk),
                      (unsigned int)data + offset);
        toggle ^= 1u;
        offset += chunk;
        ++td_count;
    }

    uhci_td_setup(td_count, UHCI_PTR_TERM, status,
                  uhci_token(dir_in ? USB_PID_OUT : USB_PID_IN, dev->addr, 0u,
                             1u, 0u),
                  0u);
    ++td_count;
    return uhci_run_chain(dev->io, td_count);
}

static int uhci_bulk(struct usb_dev* dev, unsigned char ep,
                     unsigned short mps, unsigned char dir_in, void* data,
                     unsigned int len, unsigned char* toggle_ptr) {
    unsigned int td_count = 0;
    unsigned int offset = 0;
    unsigned int status = UHCI_TD_ACTIVE | UHCI_TD_ERR3 |
                          (dev->low_speed ? UHCI_TD_LOW_SPEED : 0u);
    unsigned char pid = dir_in ? USB_PID_IN : USB_PID_OUT;

    while (offset < len) {
        unsigned int chunk = len - offset;
        unsigned int link;
        if (chunk > mps) {
            chunk = mps;
        }
        if (td_count >= USB_MAX_TD) {
            return -1;
        }
        link = (offset + chunk < len)
                   ? (uhci_td_phys(td_count + 1u) | UHCI_PTR_DEPTH)
                   : UHCI_PTR_TERM;
        uhci_td_setup(td_count, link, status,
                      uhci_token(pid, dev->addr, ep, *toggle_ptr, chunk),
                      (unsigned int)data + offset);
        *toggle_ptr ^= 1u;
        offset += chunk;
        ++td_count;
    }
    return uhci_run_chain(dev->io, td_count);
}

static int uhci_reset_port(unsigned short io, unsigned char port_index,
                           unsigned char* low_speed) {
    unsigned short port = (unsigned short)(io + UHCI_PORTSC1 + port_index * 2u);
    unsigned short st = inw(port);

    serial_write_string("UHCI port");
    serial_write_u32(port_index);
    serial_write_string("=");
    serial_write_hex16(st);
    serial_write_string("\r\n");
    if ((st & UHCI_PORT_CCS) == 0u) {
        return -1;
    }

    outw(port, (unsigned short)(st | UHCI_PORT_PR | UHCI_PORT_CHANGE));
    delay_approx_ms(50u);
    st = inw(port);
    outw(port, (unsigned short)((st & ~UHCI_PORT_PR) | UHCI_PORT_CHANGE));
    delay_approx_ms(10u);
    st = inw(port);
    outw(port, (unsigned short)(st | UHCI_PORT_PE | UHCI_PORT_CHANGE));
    delay_approx_ms(20u);
    st = inw(port);

    serial_write_string("UHCI port");
    serial_write_u32(port_index);
    serial_write_string("*=");
    serial_write_hex16(st);
    serial_write_string("\r\n");
    if ((st & UHCI_PORT_PE) == 0u) {
        return -1;
    }
    *low_speed = (st & UHCI_PORT_LSDA) != 0u ? 1u : 0u;
    return 0;
}

static int uhci_controller_reset(unsigned short io) {
    unsigned int timeout = 1000000u;
    outw((unsigned short)(io + UHCI_USBCMD), UHCI_CMD_HCRESET);
    while ((inw((unsigned short)(io + UHCI_USBCMD)) & UHCI_CMD_HCRESET) != 0u) {
        if (--timeout == 0u) {
            return -1;
        }
    }
    delay_approx_ms(10u);
    uhci_prepare_schedule(io);
    return 0;
}

static int usb_get_descriptor(struct usb_dev* dev, unsigned char type,
                              unsigned char index, void* data,
                              unsigned int len) {
    return uhci_control(dev, 0x80u, 0x06u,
                        (unsigned short)(((unsigned short)type << 8) | index),
                        0u, data, len, 1u);
}

static int usb_set_address(struct usb_dev* dev, unsigned char addr) {
    if (uhci_control(dev, 0x00u, 0x05u, addr, 0u, 0, 0u, 0u) != 0) {
        return -1;
    }
    delay_approx_ms(10u);
    dev->addr = addr;
    return 0;
}

static int usb_set_configuration(struct usb_dev* dev, unsigned char cfg) {
    return uhci_control(dev, 0x00u, 0x09u, cfg, 0u, 0, 0u, 0u);
}

static int usb_parse_config(struct usb_dev* dev, unsigned char* cfg,
                            unsigned int total) {
    unsigned int off = 0;
    unsigned char in_mass = 0;
    unsigned char cfg_value = cfg[5];

    dev->bulk_in = 0;
    dev->bulk_out = 0;
    dev->bulk_in_mps = 0;
    dev->bulk_out_mps = 0;

    while (off + 2u <= total) {
        unsigned char len = cfg[off];
        unsigned char type = cfg[off + 1u];
        if (len < 2u || off + len > total) {
            break;
        }
        if (type == 0x04u && len >= 9u) {
            in_mass = (cfg[off + 5u] == 0x08u && cfg[off + 7u] == 0x50u)
                          ? 1u
                          : 0u;
            if (in_mass) {
                dev->interface_number = cfg[off + 2u];
                serial_write_string("USB MSC if=");
                serial_write_hex8(dev->interface_number);
                serial_write_string(" sub=");
                serial_write_hex8(cfg[off + 6u]);
                serial_write_string("\r\n");
            }
        } else if (type == 0x05u && len >= 7u && in_mass) {
            unsigned char ep = cfg[off + 2u];
            unsigned char attr = cfg[off + 3u];
            unsigned short mps = le16(cfg + off + 4u);
            if ((attr & 0x03u) == 0x02u) {
                if ((ep & 0x80u) != 0u) {
                    dev->bulk_in = (unsigned char)(ep & 0x0fu);
                    dev->bulk_in_mps = mps;
                } else {
                    dev->bulk_out = (unsigned char)(ep & 0x0fu);
                    dev->bulk_out_mps = mps;
                }
            }
        }
        off += len;
    }

    if (dev->bulk_in == 0u || dev->bulk_out == 0u || dev->bulk_in_mps == 0u ||
        dev->bulk_out_mps == 0u) {
        return -1;
    }

    serial_write_string("USB bulk in=");
    serial_write_hex8(dev->bulk_in);
    serial_write_string(" out=");
    serial_write_hex8(dev->bulk_out);
    serial_write_string(" mps=");
    serial_write_hex16(dev->bulk_in_mps);
    serial_write_char('/');
    serial_write_hex16(dev->bulk_out_mps);
    serial_write_string(" cfg=");
    serial_write_hex8(cfg_value);
    serial_write_string("\r\n");

    if (usb_set_configuration(dev, cfg_value) != 0) {
        return -1;
    }
    dev->bulk_in_toggle = 0;
    dev->bulk_out_toggle = 0;
    delay_approx_ms(50u);
    return 0;
}

static int usb_enumerate_device(struct usb_dev* dev) {
    unsigned char* desc = (unsigned char*)USB_CFG_BUF_LINEAR;
    unsigned int total;

    dev->addr = 0;
    dev->ep0_mps = 8u;
    if (usb_get_descriptor(dev, 0x01u, 0u, desc, 8u) != 0) {
        return -1;
    }
    if (desc[0] < 8u || desc[1] != 0x01u) {
        return -1;
    }
    dev->ep0_mps = desc[7];
    if (dev->ep0_mps != 8u && dev->ep0_mps != 16u && dev->ep0_mps != 32u &&
        dev->ep0_mps != 64u) {
        dev->ep0_mps = 8u;
    }
    serial_write_string("USB ep0=");
    serial_write_u32(dev->ep0_mps);
    serial_write_string("\r\n");

    if (usb_set_address(dev, 1u) != 0) {
        return -1;
    }
    if (usb_get_descriptor(dev, 0x01u, 0u, desc, 18u) != 0) {
        return -1;
    }
    serial_write_string("USB dev ");
    serial_write_hex16(le16(desc + 8u));
    serial_write_char(':');
    serial_write_hex16(le16(desc + 10u));
    serial_write_string("\r\n");

    if (usb_get_descriptor(dev, 0x02u, 0u, desc, 9u) != 0) {
        return -1;
    }
    total = le16(desc + 2u);
    if (total > 512u) {
        total = 512u;
    }
    if (usb_get_descriptor(dev, 0x02u, 0u, desc, total) != 0) {
        return -1;
    }
    return usb_parse_config(dev, desc, total);
}

static int usb_msd_command(struct usb_dev* dev, const unsigned char* cdb,
                           unsigned int cdb_len, unsigned char dir_in,
                           void* data, unsigned int data_len) {
    unsigned char* cbw = (unsigned char*)USB_CBW_BUF_LINEAR;
    unsigned char* csw = (unsigned char*)USB_CSW_BUF_LINEAR;
    unsigned int tag = 0x440b0001u;
    unsigned int i;

    storage_memset(cbw, 0u, 31u);
    put32le(cbw + 0u, 0x43425355u);
    put32le(cbw + 4u, tag);
    put32le(cbw + 8u, data_len);
    cbw[12] = dir_in ? 0x80u : 0x00u;
    cbw[13] = 0u;
    cbw[14] = (unsigned char)cdb_len;
    for (i = 0; i < cdb_len && i < 16u; ++i) {
        cbw[15u + i] = cdb[i];
    }

    if (uhci_bulk(dev, dev->bulk_out, dev->bulk_out_mps, 0u, cbw, 31u,
                  &dev->bulk_out_toggle) != 0) {
        return -1;
    }
    if (data_len != 0u) {
        if (dir_in) {
            if (uhci_bulk(dev, dev->bulk_in, dev->bulk_in_mps, 1u, data,
                          data_len, &dev->bulk_in_toggle) != 0) {
                return -1;
            }
        } else {
            if (uhci_bulk(dev, dev->bulk_out, dev->bulk_out_mps, 0u, data,
                          data_len, &dev->bulk_out_toggle) != 0) {
                return -1;
            }
        }
    }
    storage_memset(csw, 0u, 16u);
    if (uhci_bulk(dev, dev->bulk_in, dev->bulk_in_mps, 1u, csw, 13u,
                  &dev->bulk_in_toggle) != 0) {
        return -1;
    }
    if (le32(csw) != 0x53425355u || le32(csw + 4u) != tag || csw[12] != 0u) {
        if (usb_msd_quiet_status) {
            return -1;
        }
        serial_write_string("USB CSW bad sig=");
        serial_write_hex32(le32(csw));
        serial_write_string(" tag=");
        serial_write_hex32(le32(csw + 4u));
        serial_write_string(" st=");
        serial_write_hex8(csw[12]);
        serial_write_string("\r\n");
        return -1;
    }
    return 0;
}

static void usb_msd_request_sense(struct usb_dev* dev) {
    unsigned char cdb[6];
    unsigned char* sense = (unsigned char*)USB_CFG_BUF_LINEAR;

    storage_memset(cdb, 0u, sizeof(cdb));
    storage_memset(sense, 0u, 18u);
    cdb[0] = 0x03u;
    cdb[4] = 18u;
    if (usb_msd_command(dev, cdb, 6u, 1u, sense, 18u) == 0) {
        serial_dump_bytes("USB sense", sense, 18u);
    }
}

static void usb_msd_read_capacity(struct usb_dev* dev) {
    unsigned char cdb[10];
    unsigned char* cap = (unsigned char*)USB_CFG_BUF_LINEAR;

    storage_memset(cdb, 0u, sizeof(cdb));
    storage_memset(cap, 0u, 8u);
    cdb[0] = 0x25u;
    if (usb_msd_command(dev, cdb, 10u, 1u, cap, 8u) == 0) {
        unsigned int last_lba = ((unsigned int)cap[0] << 24) |
                                ((unsigned int)cap[1] << 16) |
                                ((unsigned int)cap[2] << 8) | cap[3];
        unsigned int block_len = ((unsigned int)cap[4] << 24) |
                                 ((unsigned int)cap[5] << 16) |
                                 ((unsigned int)cap[6] << 8) | cap[7];
        serial_write_string("USB capacity last=");
        serial_write_hex32(last_lba);
        serial_write_string(" blksz=");
        serial_write_hex32(block_len);
        serial_write_string("\r\n");
    } else {
        usb_msd_request_sense(dev);
    }
}

static void usb_msd_test_unit_ready(struct usb_dev* dev) {
    unsigned char cdb[6];

    storage_memset(cdb, 0u, sizeof(cdb));
    cdb[0] = 0x00u;
    usb_msd_quiet_status = 1u;
    if (usb_msd_command(dev, cdb, 6u, 0u, 0, 0u) != 0) {
        usb_msd_quiet_status = 0u;
        usb_msd_request_sense(dev);
    } else {
        usb_msd_quiet_status = 0u;
    }
}

static int usb_msd_read_lba0(struct usb_dev* dev, unsigned char* sector) {
    unsigned char cdb[10];
    unsigned int attempt;

    usb_msd_test_unit_ready(dev);
    usb_msd_read_capacity(dev);

    storage_memset(cdb, 0u, sizeof(cdb));
    cdb[0] = 0x28u;
    cdb[8] = 1u;
    for (attempt = 0; attempt < 3u; ++attempt) {
        if (usb_msd_command(dev, cdb, 10u, 1u, sector, 512u) == 0) {
            return 0;
        }
        usb_msd_request_sense(dev);
    }
    return -1;
}

static void usb_scan(void) {
    unsigned int bar4;
    unsigned short io;
    unsigned char port;

    if (!storage_uhci_found) {
        serial_write_string("USB: UHCI not found\r\n");
        return;
    }
    pci_write16(storage_uhci_bdf.bus, storage_uhci_bdf.dev, storage_uhci_bdf.fn,
                0x04u,
                (unsigned short)(pci_read16(storage_uhci_bdf.bus,
                                            storage_uhci_bdf.dev,
                                            storage_uhci_bdf.fn, 0x04u) |
                                 0x0005u));
    bar4 = pci_read32(storage_uhci_bdf.bus, storage_uhci_bdf.dev,
                      storage_uhci_bdf.fn, 0x20u);
    io = (unsigned short)(bar4 & 0xffe0u);
    if (io == 0u) {
        io = (unsigned short)align_up_u32(pci_next_io, 0x20u);
        pci_next_io = io + 0x20u;
        pci_write32(storage_uhci_bdf.bus, storage_uhci_bdf.dev,
                    storage_uhci_bdf.fn, 0x20u, (unsigned int)io | 1u);
    }

    serial_write_string("USB UHCI ");
    pci_print_bdf(storage_uhci_bdf.bus, storage_uhci_bdf.dev,
                  storage_uhci_bdf.fn);
    serial_write_string(" io=");
    serial_write_hex16(io);
    serial_write_string("\r\n");

    if (uhci_controller_reset(io) != 0) {
        serial_write_string("UHCI reset failed\r\n");
        return;
    }

    for (port = 0; port < 2u; ++port) {
        struct usb_dev dev;
        unsigned char low_speed = 0;
        unsigned char* sector = (unsigned char*)USB_SECTOR_LINEAR;
        if (uhci_reset_port(io, port, &low_speed) != 0) {
            continue;
        }
        storage_memset(&dev, 0u, sizeof(dev));
        dev.io = io;
        dev.low_speed = low_speed;
        if (usb_enumerate_device(&dev) != 0) {
            serial_write_string("USB enum failed\r\n");
            continue;
        }
        if (usb_msd_read_lba0(&dev, sector) == 0) {
            serial_dump_bytes("USB LBA0", sector, 16u);
        } else {
            serial_write_string("USB LBA0 read failed\r\n");
        }
    }
}

static void storage_scan(unsigned int total_bytes) {
    if (total_bytes < 0x00800000u) {
        serial_write_string("Storage scan skipped: low DRAM\r\n");
        return;
    }
    pci_enumerate_and_assign();
    ide_scan();
    usb_scan();
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
};

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

#define FDOS_BLOB_LINEAR 0x00120000u
#define TEST10_BLOB_LINEAR 0x00130000u
#define TESTFD_BLOB_LINEAR 0x00140000u
#define RAM_FLOPPY_LINEAR 0x00200000u
#define RAM_FLOPPY_CAPACITY 0x00200000u
#define BOOT_SECTOR_LINEAR 0x00007c00u

static unsigned char* bios_floppy_image = (unsigned char*)RAM_FLOPPY_LINEAR;
static unsigned int bios_floppy_total_sectors = 0;

static unsigned int rm_seg_off_to_linear(unsigned short seg,
                                         unsigned short off) {
    return ((unsigned int)seg << 4) + off;
}

static void rm_set_cf(struct rm_int13_frame* f) { f->flags |= 0x0001u; }

static void rm_clear_cf(struct rm_int13_frame* f) { f->flags &= 0xfffeu; }

static int expand_fdos_blob_to_dram(const unsigned char* blob) {
    struct blob_status status;
    blob_expand_fn expand = (blob_expand_fn)BLOB_SERVICE_LINEAR;
    int rc = expand(blob, (void*)BLOB_STAGE_LINEAR,
                    (void*)RAM_FLOPPY_LINEAR, RAM_FLOPPY_CAPACITY, &status);

    if (rc != 0) {
        serial_write_string("FDOS blob err rc=");
        serial_write_hex32((unsigned int)rc);
        serial_write_string(" code=");
        serial_write_hex32((unsigned int)status.code);
        serial_write_string(" blk=");
        serial_write_hex32(status.block);
        serial_write_string(" exp=");
        serial_write_hex32(status.expected);
        serial_write_string(" got=");
        serial_write_hex32(status.got);
        serial_write_string("\r\n");
        if (status.code == BLOB_ERR_CRC) {
            const struct blob_header* hdr = (const struct blob_header*)blob;
            const struct blob_block* blocks =
                (const struct blob_block*)(blob + hdr->block_table_off);
            const struct blob_block* block = blocks + status.block;
            const unsigned char* src = blob + hdr->data_off +
                                       block->compressed_off;
            const unsigned char* stage =
                (const unsigned char*)BLOB_STAGE_LINEAR;
            serial_write_string("FDOS crc blk src=");
            serial_write_hex32((unsigned int)src);
            serial_write_string(" stage=");
            serial_write_hex32((unsigned int)stage);
            serial_write_string(" csz=");
            serial_write_hex32(block->compressed_size);
            serial_write_string("\r\n");
            serial_dump_bytes("src", src, 32u);
            serial_dump_bytes("stage", stage, 32u);
            serial_dump_bytes("src2", src, 32u);
        }
        return -1;
    }
    if ((status.output_size & 511u) != 0u) {
        serial_write_string("FDOS blob size not sector aligned\r\n");
        return -1;
    }
    bios_floppy_total_sectors = status.output_size >> 9;
    return 0;
}

static void copy_boot_sector_from_ram_floppy(void) {
    unsigned int i;
    unsigned char* boot = (unsigned char*)BOOT_SECTOR_LINEAR;

    for (i = 0; i < 512u; ++i) {
        boot[i] = bios_floppy_image[i];
    }
}

static void rm_set_zf(struct rm_int13_frame* f) { f->flags |= 0x0040u; }

static void rm_clear_zf(struct rm_int13_frame* f) { f->flags &= ~0x0040u; }

static int bios_hdd_read_sectors(unsigned int lba, unsigned int count,
                                 unsigned int dest) {
    unsigned int i;

    if (!bios_hdd_present || count == 0u ||
        lba + count > bios_hdd_total_sectors) {
        serial_write_string("13h:r range fail total=");
        serial_write_hex32(bios_hdd_total_sectors);
        serial_write_string("\r\n");
        return -1;
    }
    for (i = 0; i < count; ++i) {
        if (ide_read_lba28(bios_hdd_io, bios_hdd_ctrl, bios_hdd_drive, lba + i,
                           (unsigned char*)(dest + i * 512u)) != 0) {
            serial_write_string("13h:r pio fail lba=");
            serial_write_hex32(lba + i);
            serial_write_string(" st=");
            serial_write_hex8(inb((unsigned short)(bios_hdd_io + 7u)));
            serial_write_string(" err=");
            serial_write_hex8(inb((unsigned short)(bios_hdd_io + 1u)));
            serial_write_string("\r\n");
            return -1;
        }
    }
    return 0;
}

static void bios_int13_hdd_params(struct rm_int13_frame* f) {
    unsigned int max_cyl = bios_hdd_cylinders - 1u;
    unsigned int max_head = bios_hdd_heads - 1u;
    unsigned int spt = bios_hdd_spt;

    f->ax = 0u;
    f->cx = (unsigned short)(((max_cyl & 0xffu) << 8) | spt |
                             ((max_cyl >> 2) & 0xc0u));
    f->dx = (unsigned short)((max_head << 8) | 0x01u);
    rm_clear_cf(f);
}

static void bios_int13_hdd_chs_rw(struct rm_int13_frame* f,
                                  unsigned char ah) {
    unsigned int count = (unsigned char)f->ax;
    unsigned int cylinder = ((unsigned int)(f->cx >> 8) |
                             (((unsigned int)f->cx & 0x00c0u) << 2));
    unsigned int sector = (unsigned int)(f->cx & 0x003fu);
    unsigned int head = (unsigned int)(f->dx >> 8);
    unsigned int lba;
    unsigned int dest;

    if (ah != 0x02u || sector == 0u || count == 0u ||
        head >= bios_hdd_heads || cylinder >= bios_hdd_cylinders) {
        f->ax = 0x0100u;
        rm_set_cf(f);
        return;
    }

    lba = ((cylinder * bios_hdd_heads) + head) * bios_hdd_spt + sector - 1u;
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
    struct rm_dap* dap =
        (struct rm_dap*)rm_seg_off_to_linear(f->ds, f->si);
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
    unsigned int fill_size;

    if (p->size < 0x1au) {
        f->ax = 0x0100u;
        rm_set_cf(f);
        return;
    }

    fill_size = p->size;
    if (fill_size > sizeof(*p)) {
        fill_size = sizeof(*p);
    }
    storage_memset(p, 0, fill_size);
    p->size = (unsigned short)fill_size;
    p->information = 0x0001u;
    p->cylinders = bios_hdd_cylinders;
    p->heads = bios_hdd_heads;
    p->sectors = bios_hdd_spt;
    p->total_sectors_low = bios_hdd_total_sectors;
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

    if (dl != 0x80u || !bios_hdd_present) {
        f->ax = 0x0100u;
        rm_set_cf(f);
        return;
    }

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
            f->cx = (unsigned short)(bios_hdd_total_sectors >> 16);
            f->dx = (unsigned short)bios_hdd_total_sectors;
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
    unsigned char ah = (unsigned char)(f->ax >> 8);
    unsigned char al = (unsigned char)(f->ax & 0xffu);
    unsigned char dl = (unsigned char)(f->dx & 0xffu);

    if (dl >= 0x80u) {
        bios_int13_hdd_service(f);
        return;
    }

    if (bios_floppy_total_sectors == 0) {
        serial_write_string("13:noimg\r\n");
        f->ax = 0x0100u;
        rm_set_cf(f);
        return;
    }

    if (dl != 0x00u) {
        f->ax = 0x0100u;
        rm_set_cf(f);
        return;
    }

    if (0) {
        serial_write_string("13:ah=");
        serial_write_hex8(ah);
        serial_write_string(" dl=");
        serial_write_hex8(dl);
        serial_write_string("\r\n");
    }

    int drive;
    switch (ah) {
        case 0x00:
            drive = f->dx & 0x7f;
            if (drive == 0) {
                f->ax &= 0x00ffu;
                rm_clear_cf(f);
            } else {
                f->ax = 0x0100u;
                rm_set_cf(f);
            }
            return;
        case 0x08:
            drive = f->dx & 0xff;
            if (drive == 0) {
                int num_head = 2;
                int num_cylinder = 80;
                int num_sector = 18;
                f->ax = 0;
                f->bx = 4;
                f->cx = (num_cylinder << 8) | num_sector;
                f->dx = (num_head << 8) | 1;

                rm_clear_cf(f);
            } else {
                f->ax = 0x0100u;
                rm_set_cf(f);
            }
            return;
        case 0x15:
            drive = f->dx & 0xff;
            if (drive == 0) {
                f->ax = (unsigned short)((1u << 8) | (f->ax & 0x00ffu));
                rm_clear_cf(f);
            } else {
                f->ax = 0x0100u;
                rm_set_cf(f);
            }
            return;
        case 0x02:
        case 0x03: {
            unsigned int lba;
            unsigned int dest;
            unsigned int count = al;
            unsigned int i;
            unsigned int cylinder = (unsigned int)(f->cx >> 8);
            unsigned int sector = (unsigned int)(f->cx & 0x3f) - 1u;
            unsigned int head = (unsigned int)(f->dx >> 8);

            drive = f->dx & 0xff;
            if (drive != 0) {
                f->ax = 0x0100u;
                rm_set_cf(f);
                break;
            }

            int num_head = 2;
            int num_sector = 18;
            lba = sector;
            lba += head * num_sector;
            lba += cylinder * num_head * num_sector;

            if (lba + count > bios_floppy_total_sectors) {
                f->ax = 0x0100u;
                rm_set_cf(f);
                break;
            }
            dest = rm_seg_off_to_linear(f->es, f->bx);
            // serial_write_string("lba=");
            // serial_write_hex32(lba);
            // serial_write_string(",dest=");
            // serial_write_hex32(dest);
            // serial_write_string("\r\n");

            for (i = 0; i < count; ++i) {
                unsigned int j;
                unsigned char* mem = (unsigned char*)(dest + i * 512u);
                unsigned char* img = bios_floppy_image + (lba + i) * 512u;
                if (ah == 0x02u) {
                    for (j = 0; j < 512u; ++j) {
                        mem[j] = img[j];
                    }
                } else {
                    for (j = 0; j < 512u; ++j) {
                        img[j] = mem[j];
                    }
                }
            }
            f->ax = (unsigned short)count;
            rm_clear_cf(f);
            return;
        }
        default:
            break;
    }

    serial_write_string("13:bad ah=");
    serial_write_hex8(ah);
    serial_write_string(", cx=");
    serial_write_hex16(f->cx);
    serial_write_string(", dx=");
    serial_write_hex16(f->dx);
    serial_write_string("\r\n");
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
            f->ax = 0x0001u;
            return;
        case 0x12:
            f->ax = bios_dos_base_mem_kb;
            return;
        case 0x13:
        case 0x40:
            bios_int13_service(f);
            return;
        case 0x15:
            if ((unsigned char)(f->ax >> 8) == 0x88u) {
                unsigned int ext_kb = 0;
                if (bios_total_bytes_global > 0x00100000u) {
                    ext_kb = (bios_total_bytes_global - 0x00100000u) >> 10;
                }
                if (ext_kb > 0xffffu) {
                    ext_kb = 0xffffu;
                }
                f->ax = (unsigned short)ext_kb;
                rm_clear_cf(f);
                return;
            }
            if (f->ax == 0xe801u) {
                unsigned int ext_kb = 0;
                unsigned int below16m_kb = 0;
                unsigned int above16m_64k = 0;
                if (bios_total_bytes_global > 0x00100000u) {
                    ext_kb = (bios_total_bytes_global - 0x00100000u) >> 10;
                }
                below16m_kb = ext_kb;
                if (below16m_kb > 15u * 1024u) {
                    below16m_kb = 15u * 1024u;
                }
                if (ext_kb > below16m_kb) {
                    above16m_64k = (ext_kb - below16m_kb) >> 6;
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
            copy_boot_sector_from_ram_floppy();
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

void postcar_resume(unsigned int total_bytes, unsigned int fdos_blob_linear) {
    volatile unsigned int stack_cookie = 0x13579bdfu;

    bios_total_bytes_global = total_bytes;
    outb(0x80, POST_DRAM_STACK);
    serial_write_string("BIOS.elf @ 00100000\r\n");
    serial_write_string("post-CAR ok\r\n");
    serial_write_string("DRAM stack @ ");
    serial_write_hex16((unsigned short)(((unsigned int)&stack_cookie) >> 16));
    serial_write_hex16((unsigned short)((unsigned int)&stack_cookie));
    serial_write_string("\r\n");
    serial_write_string("Usable DRAM: ");
    serial_write_u32(total_bytes >> 10);
    serial_write_string("K\r\n");
    storage_scan(total_bytes);
    {
        serial_write_string("Expand FDOS...\r\n");
        serial_write_string("FDOS blob @ ");
        serial_write_hex32(fdos_blob_linear);
        serial_write_string(" magic=");
        serial_write_hex32(*(const unsigned int*)fdos_blob_linear);
        serial_write_string("\r\n");
        if (expand_fdos_blob_to_dram(
                (const unsigned char*)fdos_blob_linear) != 0) {
            serial_write_string("FreeDOS image expand failed\r\n");
            for (;;) {
                __asm__ volatile("hlt");
            }
        }
    }
    serial_write_string("FDOS expanded\r\n");
    copy_boot_sector_from_ram_floppy();
    install_bios_thunks();
    *(volatile unsigned int*)(bios16_thunk_runtime_base +
                              (unsigned int)((unsigned char*)&bios16_pm_stack_top -
                                             bios16_thunk_start)) =
        ((unsigned int)&stack_cookie & ~0x0fu) - 0x400u;
    serial_write_string("IVT thunks installed @ ");
    serial_write_hex32(bios16_thunk_runtime_base);
    serial_write_string("\r\n");
    serial_write_string("Booting FreeDOS...\r\n");
    bios_boot_freedos_pm32();
    serial_write_string("FreeDOS returned\r\n");
    bandwidth_benchmarks(total_bytes);
    for (;;) {
        __asm__ volatile("hlt");
    }
}

void bios32_entry_c(unsigned int total_bytes, unsigned int fdos_blob_linear) {
    zero_bss();
    postcar_resume(total_bytes, fdos_blob_linear);
}

void bios32_qemu_entry(unsigned int total_bytes, unsigned int fdos_blob_linear) {
    volatile unsigned int stack_cookie = 0x2468ace0u;

    zero_bss();
    bios_total_bytes_global = total_bytes;
    if (fdos_blob_linear == 0u) {
        fdos_blob_linear = FDOS_BLOB_LINEAR;
    }
    if (expand_fdos_blob_to_dram((const unsigned char*)fdos_blob_linear) != 0) {
        serial_write_string("QEMU FreeDOS expand failed\r\n");
        for (;;) {
            __asm__ volatile("hlt");
        }
    }
    copy_boot_sector_from_ram_floppy();
    serial_write_string("QEMU BIOS.elf @ 00100000\r\n");
    serial_write_string("QEMU stack @ ");
    serial_write_hex32((unsigned int)&stack_cookie);
    serial_write_string("\r\n");
    storage_scan(total_bytes);
    install_bios_thunks();
    *(volatile unsigned int*)(bios16_thunk_runtime_base +
                              (unsigned int)((unsigned char*)&bios16_pm_stack_top -
                                             bios16_thunk_start)) =
        ((unsigned int)&stack_cookie & ~0x0fu) - 0x400u;
    serial_write_string("QEMU boot FreeDOS...\r\n");
    bios_boot_freedos_pm32();
    serial_write_string("QEMU FreeDOS returned\r\n");
    bandwidth_benchmarks(total_bytes);
    for (;;) {
        __asm__ volatile("hlt");
    }
}
