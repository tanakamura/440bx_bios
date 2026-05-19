#include "legacy_video.h"
#include "legacy_platform.h"

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

static unsigned short* cursor_slot(unsigned char page) {
    return (unsigned short*)(BDA_CURSOR_POS +
                             ((unsigned short)(page & 7u) * 2u));
}

void legacy_video_init(void) {
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
        *cursor_slot((unsigned char)i) = 0x0000u;
    }
}

static unsigned short get_cursor(unsigned char page) {
    return *cursor_slot(page);
}

static void set_cursor(unsigned char page, unsigned char row,
                       unsigned char col) {
    *cursor_slot(page) = (unsigned short)(((unsigned short)row << 8) | col);
}

static void tty_advance(unsigned char ch) {
    unsigned char page = *(volatile unsigned char*)BDA_ACTIVE_PAGE;
    unsigned short cur = get_cursor(page);
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
    set_cursor(page, row, col);
}

void legacy_int10_service(struct rm_int13_frame* f) {
    unsigned char ah = (unsigned char)(f->ax >> 8);

    if (ah == 0x0eu) {
        legacy_serial_write_char((char)(f->ax & 0xffu));
        tty_advance((unsigned char)(f->ax & 0xffu));
        return;
    }
    if (ah == 0x02u) {
        set_cursor((unsigned char)(f->bx >> 8), (unsigned char)(f->dx >> 8),
                   (unsigned char)(f->dx & 0x00ffu));
        return;
    }
    if (ah == 0x03u) {
        f->dx = get_cursor((unsigned char)(f->bx >> 8));
        f->cx = *(volatile unsigned short*)BDA_CURSOR_SHAPE;
        return;
    }
    if (ah == 0x0fu) {
        f->ax = (unsigned short)((80u << 8) | 0x03u);
        f->bx &= 0x00ffu;
    }
}
