#include "app/legacy/legacy_io.h"
#include "legacy_keyboard.h"

#define BDA_KBD_FLAGS1 0x0417u
#define BDA_KBD_FLAGS2 0x0418u
#define BDA_KBD_HEAD 0x041au
#define BDA_KBD_TAIL 0x041cu
#define BDA_KBD_BUF_START 0x0480u
#define BDA_KBD_BUF_END 0x0482u
#define BDA_KBD_BUF_BASE 0x041eu
#define BDA_KBD_BUF_LIMIT 0x003eu

static void rm_clear_cf(struct rm_int13_frame* f) { f->flags &= 0xfffeu; }

static void rm_set_zf(struct rm_int13_frame* f) { f->flags |= 0x0040u; }

static void rm_clear_zf(struct rm_int13_frame* f) { f->flags &= ~0x0040u; }

static unsigned char ascii_scan_code(unsigned char ch) {
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
        return ascii_scan_code((unsigned char)(ch - 'A' + 'a'));
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

static unsigned short translate_serial_key(unsigned char ch) {
    if (ch == '\r' || ch == '\n') {
        return 0x1c0du;
    }
    if (ch == 0x08u || ch == 0x7fu) {
        return 0x0e08u;
    }
    if (ch == 0x1bu) {
        return 0x011bu;
    }
    return (unsigned short)(((unsigned short)ascii_scan_code(ch) << 8) | ch);
}

void legacy_keyboard_init(void) {
    *(volatile unsigned char*)BDA_KBD_FLAGS1 = 0x00u;
    *(volatile unsigned char*)BDA_KBD_FLAGS2 = 0x00u;
    *(volatile unsigned short*)BDA_KBD_HEAD = 0x001eu;
    *(volatile unsigned short*)BDA_KBD_TAIL = 0x001eu;
    *(volatile unsigned short*)BDA_KBD_BUF_START = 0x001eu;
    *(volatile unsigned short*)BDA_KBD_BUF_END = 0x003eu;
}

static int kbd_buf_nonempty(void) {
    return *(volatile unsigned short*)BDA_KBD_HEAD !=
           *(volatile unsigned short*)BDA_KBD_TAIL;
}

static int kbd_enqueue(unsigned short ax) {
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

static unsigned short kbd_dequeue(void) {
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

static int try_fill_keybuf(void) {
    if (kbd_buf_nonempty()) {
        return 1;
    }
    if ((inb(0x03fd) & 0x01u) == 0) {
        return 0;
    }
    return kbd_enqueue(translate_serial_key(inb(0x03f8)));
}

void legacy_int16_service(struct rm_int13_frame* f) {
    switch ((unsigned char)(f->ax >> 8)) {
        case 0x01u:
        case 0x11u:
            if (!try_fill_keybuf()) {
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
                                     *(volatile unsigned char*)BDA_KBD_FLAGS1);
            rm_clear_cf(f);
            return;
        case 0x12u:
            f->ax =
                (unsigned short)((*(volatile unsigned char*)BDA_KBD_FLAGS2
                                  << 8) |
                                 *(volatile unsigned char*)BDA_KBD_FLAGS1);
            rm_clear_cf(f);
            return;
        case 0x00u:
        case 0x10u:
        default:
            while (!try_fill_keybuf()) {
            }
            f->ax = kbd_dequeue();
            rm_clear_cf(f);
            return;
    }
}
