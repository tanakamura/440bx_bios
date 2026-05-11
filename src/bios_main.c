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

__attribute__((section(".entry"))) void bios32_entry(unsigned int total_bytes);
__attribute__((section(".qentry"))) void bios32_qemu_entry(
    unsigned int total_bytes);
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

static unsigned int bios_total_bytes_global = 0;
static const unsigned int bios16_thunk_runtime_base = 0x0009fc00u;
static const unsigned short bios_base_mem_kb =
    (unsigned short)(0x0009fc00u >> 10);
static const unsigned short bios_ebda_segment = 0x9fc0u;
static const unsigned short bios_dos_base_mem_kb = 256u;
static unsigned int bios_floppy_dpt_linear = 0x00000500u;
static unsigned char bios_kbd_pending_valid = 0;
static unsigned short bios_kbd_pending_ax = 0;
static unsigned int bios_tick_counter = 0;
static unsigned char bios_tick_initialized = 0;
static unsigned short bios_tick_last_raw = 0;
static unsigned int bios_tick_subcount = 0;

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
    *(volatile unsigned char*)0x0475u = 0x00u;
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
};

#define FDOS_BLOB_LINEAR 0x00120000u
#define TEST10_BLOB_LINEAR 0x00130000u
#define TESTFD_BLOB_LINEAR 0x00140000u
#define RAM_FLOPPY_LINEAR 0x00200000u
#define RAM_FLOPPY_CAPACITY 0x00200000u
#define BOOT_SECTOR_LINEAR 0x00007c00u
#define HANDOFF_FDOS_BLOB_LINEAR 0x00080040u

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

static void bios_int13_service(struct rm_int13_frame* f) {
    unsigned char ah = (unsigned char)(f->ax >> 8);
    unsigned char al = (unsigned char)(f->ax & 0xffu);
    unsigned char dl = (unsigned char)(f->dx & 0xffu);

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

void postcar_resume(unsigned int total_bytes) {
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
    {
        unsigned int fdos_blob_linear =
            *(volatile unsigned int*)HANDOFF_FDOS_BLOB_LINEAR;
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

void bios32_entry(unsigned int total_bytes) { postcar_resume(total_bytes); }

void bios32_qemu_entry(unsigned int total_bytes) {
    volatile unsigned int stack_cookie = 0x2468ace0u;
    unsigned int fdos_blob_linear =
        *(volatile unsigned int*)HANDOFF_FDOS_BLOB_LINEAR;

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
