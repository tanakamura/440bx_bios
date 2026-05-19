#include <conio.h>
#include <dos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ROM_C_LINEAR 0x000C0000UL
#define ROM_F_LINEAR 0x000F0000UL
#define ROM_TOP_LINEAR 0xFFF00000UL
#define FLASH_SIZE (256UL * 1024UL)

void __cdecl port_inpd_words(unsigned short port, unsigned short* lo,
                             unsigned short* hi);
void __cdecl port_outpd_words(unsigned short port, unsigned short lo,
                              unsigned short hi);
static unsigned char bios_flat_read8(unsigned long addr);
static void bios_flat_write8(unsigned long addr, unsigned char value);
static unsigned short pit_read_counter0(void);
static void delay_100us(unsigned int units);

static int bios_flat_call(unsigned char op, unsigned long addr,
                          unsigned char* value) {
    union REGS inregs;
    union REGS outregs;

    memset(&inregs, 0, sizeof(inregs));
    inregs.h.ah = op;
    inregs.h.al = (value != NULL) ? *value : 0u;
    inregs.x.cx = (unsigned short)(addr >> 16);
    inregs.x.dx = (unsigned short)addr;
    int86(0x60, &inregs, &outregs);
    if (outregs.x.cflag != 0) {
        return -1;
    }
    if (value != NULL) {
        *value = outregs.h.al;
    }
    return 0;
}

static unsigned char bios_flat_read8(unsigned long addr) {
    unsigned char value = 0xffu;
    bios_flat_call(0x00u, addr, &value);
    return value;
}

static void bios_flat_write8(unsigned long addr, unsigned char value) {
    (void)bios_flat_call(0x01u, addr, &value);
}

static unsigned short pit_read_counter0(void) {
    unsigned char lo;
    unsigned char hi;
    outp(0x43, 0x00);
    lo = (unsigned char)inp(0x40);
    hi = (unsigned char)inp(0x40);
    return (unsigned short)((unsigned short)hi << 8 | lo);
}

static void delay_100us(unsigned int units) {
    unsigned long remaining = (unsigned long)units * 119UL;
    unsigned short prev = pit_read_counter0();

    while (remaining > 0) {
        unsigned short cur = pit_read_counter0();
        unsigned short delta;
        if (prev >= cur) {
            delta = (unsigned short)(prev - cur);
        } else {
            delta = (unsigned short)(prev + (0x10000UL - cur));
        }
        if (delta != 0) {
            if (remaining > (unsigned long)delta) {
                remaining -= (unsigned long)delta;
            } else {
                remaining = 0;
            }
            prev = cur;
        }
    }
}

static void pci_write_addr(unsigned char bus, unsigned char dev,
                           unsigned char fn, unsigned char reg) {
    unsigned short lo =
        (unsigned short)(((unsigned short)dev << 11) |
                         ((unsigned short)fn << 8) | (reg & 0xfcu));
    unsigned short hi = (unsigned short)(0x8000u | ((unsigned short)bus << 8));
    port_outpd_words(0x0cf8u, lo, hi);
}

static unsigned short pci_read16(unsigned char bus, unsigned char dev,
                                 unsigned char fn, unsigned char reg) {
    unsigned short lo;
    unsigned short hi;
    pci_write_addr(bus, dev, fn, reg);
    port_inpd_words(0x0cfcu, &lo, &hi);
    if ((reg & 2u) != 0) {
        return hi;
    }
    return lo;
}

static void pci_write16(unsigned char bus, unsigned char dev, unsigned char fn,
                        unsigned char reg, unsigned short value16) {
    unsigned short lo;
    unsigned short hi;

    pci_write_addr(bus, dev, fn, reg);
    port_inpd_words(0x0cfcu, &lo, &hi);
    if ((reg & 2u) != 0) {
        hi = value16;
    } else {
        lo = value16;
    }
    pci_write_addr(bus, dev, fn, reg);
    port_outpd_words(0x0cfcu, lo, hi);
}

static void flash_reset(unsigned long base) {
    bios_flat_write8(base + 0x5555UL, 0xAAu);
    bios_flat_write8(base + 0x2aaaUL, 0x55u);
    bios_flat_write8(base + 0x5555UL, 0xF0u);
}

static void flash_autoselect_enter(unsigned long base) {
    bios_flat_write8(base + 0x5555UL, 0xAAu);
    bios_flat_write8(base + 0x2aaaUL, 0x55u);
    bios_flat_write8(base + 0x5555UL, 0x90u);
}

static void flash_read_id(unsigned long base, unsigned char* vendor,
                          unsigned char* device) {
    flash_autoselect_enter(base);
    *vendor = bios_flat_read8(base + 0);
    *device = bios_flat_read8(base + 1);
    flash_reset(base);
}

static void flash_sdp_disable(unsigned long base) {
    bios_flat_write8(base + 0x5555UL, 0xAAu);
    delay(1);
    bios_flat_write8(base + 0x2aaaUL, 0x55u);
    delay(1);
    bios_flat_write8(base + 0x5555UL, 0x80u);
    delay(1);
    bios_flat_write8(base + 0x5555UL, 0xAAu);
    delay(1);
    bios_flat_write8(base + 0x2aaaUL, 0x55u);
    delay(1);
    bios_flat_write8(base + 0x5555UL, 0x20u);
    delay(1);
}

static void flash_chip_erase(unsigned long base) {
    bios_flat_write8(base + 0x5555UL, 0xAAu);
    bios_flat_write8(base + 0x2aaaUL, 0x55u);
    bios_flat_write8(base + 0x5555UL, 0x80u);
    bios_flat_write8(base + 0x5555UL, 0xAAu);
    bios_flat_write8(base + 0x2aaaUL, 0x55u);
    bios_flat_write8(base + 0x5555UL, 0x10u);
}

static int flash_wait_ff(unsigned long base, unsigned long addr) {
    unsigned long tries;
    for (tries = 0; tries < 20UL; ++tries) {
        if (bios_flat_read8(base + addr) == 0xFFu) {
            return 0;
        }
        delay(1);
    }
    for (tries = 0; tries < 32; tries++) {
        printf("?? failed [%08x] = %02x\n", (int)addr,
               (int)bios_flat_read8(base + addr));
    }
    for (tries = 0; tries < 20UL; ++tries) {
        if (bios_flat_read8(base + addr) == 0xFFu) {
            return 0;
        }
    }
    for (tries = 0; tries < 32; tries++) {
        printf("flash wait failed [%08x] = %02x\n", (int)addr,
               (int)bios_flat_read8(base + addr));
    }

    return -1;
}

static void flash_dump_window(unsigned long base, unsigned long start,
                              unsigned int count) {
    unsigned int i;
    printf("DUMP %08lX:", start);
    for (i = 0; i < count; ++i) {
        printf(" %02X", bios_flat_read8(base + start + (unsigned long)i));
    }
    printf("\r\n");
}

static int flash_program_byte(unsigned long base, unsigned long offset,
                              unsigned char value) {
    unsigned long tries;
    int match = 0;
    unsigned long ntry = 20000UL;

    bios_flat_write8(base + 0x5555UL, 0xAAu);
    bios_flat_write8(base + 0x2aaaUL, 0x55u);
    bios_flat_write8(base + 0x5555UL, 0xA0u);
    bios_flat_write8(base + offset, value);
    delay(1);

    for (tries = 0; tries < ntry; ++tries) {
        if (bios_flat_read8(base + offset) == value) {
            match++;
            if (match > 16) {
                break;
            }
        } else {
            match = 0;
        }
    }
    if (tries == ntry) {
        unsigned char rd = bios_flat_read8(base + offset);
        printf("timeout! offset=%05x failed read=%02x, expected=%02x\n",
               (int)offset, rd, value);

        return -1;
    }

    if (bios_flat_read8(base + offset) != value) {
        unsigned char rd = bios_flat_read8(base + offset);
        printf("verify failed offset=%05x failed read=%02x, expected=%02x\n",
               (int)offset, rd, value);

        return -1;
    }
    return 0;
}

#define FLASH_PAGE_SIZE 128U

static int flash_program_page(unsigned long base, unsigned long offset,
                              const unsigned char* data, unsigned int count) {
    unsigned int i, j, fail;
    unsigned int verify_try = 50;
    unsigned char rd;

    bios_flat_write8(base + 0x5555UL, 0xAAu);
    bios_flat_write8(base + 0x2aaaUL, 0x55u);
    bios_flat_write8(base + 0x5555UL, 0xA0u);
    for (i = 0; i < count; ++i) {
        delay_100us(1);
        bios_flat_write8(base + offset + (unsigned long)i, data[i]);
    }
    delay(50);

    for (i = 0; i < count; ++i) {
        fail = 1;
        for (j=0; j<verify_try; j++) {
            rd = bios_flat_read8(base + offset + (unsigned long)i);
            if (rd == data[i]) {
                fail = 0;
                break;
            }
            delay(1);
        }

        if (fail) {
            printf("verify failed page=%05x+%02x read=%02x expected=%02x\r\n",
                   (int)offset, i, rd, data[i]);
            return -1;
        }
    }
    return 0;
}

unsigned char page_buf[FLASH_PAGE_SIZE];
static int flash_write_file(const char* path) {
    FILE* fp;
    unsigned long offset;
    int ch, r;
    unsigned int page_count;
    unsigned int i;
    unsigned long start = 0;
    unsigned long end = FLASH_SIZE;

    fp = fopen(path, "rb");
    if (fp == NULL) {
        printf("open failed: %s\r\n", path);
        return 1;
    }
    fseek(fp, start, SEEK_SET);

    printf("WRITE file=%s size=%lu\r\n", path, FLASH_SIZE);
    puts("Disable SDP...");
    flash_sdp_disable(ROM_TOP_LINEAR);
    delay(10);
    puts("Erase...");
    flash_chip_erase(ROM_TOP_LINEAR);
    delay(100);
    for (offset = start; offset < end; offset++) {
        r = flash_wait_ff(ROM_TOP_LINEAR, offset);
        if (r != 0) {
            puts("erase timeout");
            fclose(fp);
            return 1;
        }
        if ((offset & 0x0FFFUL) == 0) {
            printf("erased %05lX\r\n", offset);
        }
    }
    puts("erase done");

    for (offset = start; offset < end; offset += FLASH_PAGE_SIZE) {
        page_count = FLASH_PAGE_SIZE;
        if (offset + page_count > FLASH_SIZE) {
            page_count = (unsigned int)(FLASH_SIZE - offset);
        }
        for (i = 0; i < page_count; ++i) {
            ch = fgetc(fp);
            if (ch == EOF) {
                if (ferror(fp)) {
                    printf("read failed @ %08lX\r\n", offset + i);
                    fclose(fp);
                    return 1;
                }
                ch = 0xFF;
            }
            page_buf[i] = (unsigned char)ch;
        }
        if (flash_program_page(ROM_TOP_LINEAR, offset, page_buf, page_count) !=
            0) {
            printf("program failed @ %08lX\r\n", offset);
            fclose(fp);
            return 1;
        }
        if ((offset & 0x0FFFUL) == 0) {
            printf("%05lX\r\n", offset);
        }
    }

    if (!feof(fp)) {
        while ((ch = fgetc(fp)) != EOF) {
            printf("read failed @ %08lX\r\n", offset);
            fclose(fp);
            return 1;
        }
    }

    fclose(fp);
    puts("WRITE OK");
    return 0;
}

static void print_attempt(const char* label, unsigned long linear,
                          unsigned short xbcs) {
    unsigned char before0 = bios_flat_read8(linear + 0);
    unsigned char before1 = bios_flat_read8(linear + 1);
    unsigned char vendor = 0xffu;
    unsigned char device = 0xffu;
    unsigned char b0;
    unsigned char b1;
    unsigned char b2;
    unsigned char b3;
    unsigned char iddump[8];
    unsigned int i;

    b0 = before0;
    b1 = before1;
    b2 = bios_flat_read8(linear + 2);
    b3 = bios_flat_read8(linear + 3);
    flash_autoselect_enter(linear);
    vendor = bios_flat_read8(linear + 0);
    device = bios_flat_read8(linear + 1);
    for (i = 0; i < 8; ++i) {
        iddump[i] = bios_flat_read8(linear + (unsigned long)i);
    }
    flash_reset(linear);

    printf(
        "%s linear=%08lX XBCS=%04X before=%02X %02X dword=%02X%02X%02X%02X "
        "id=%02X %02X after=%02X %02X\r\n",
        label, linear, xbcs, before0, before1, b3, b2, b1, b0, vendor, device,
        bios_flat_read8(linear + 0), bios_flat_read8(linear + 1));
    printf("  iddump:");
    for (i = 0; i < 8; ++i) {
        printf(" %02X", iddump[i]);
    }
    printf("\r\n");
}

int main(int argc, char** argv) {
    unsigned short xbcs = pci_read16(0, 7, 0, 0x4e);
    xbcs |=
        (1 << 9) | (1 << 7) | (1 << 6) | (1 << 2); /* enable extended access */
    pci_write16(0, 7, 0, 0x4e, xbcs);

    printf("FLASHUTIL\r\n");
    printf("PIIX4 00:07.0 reg4E=%04X\r\n", xbcs);
    if (argc >= 2) {
        return flash_write_file(argv[1]);
    }
    print_attempt("C alias", ROM_C_LINEAR, xbcs);
    print_attempt("F alias", ROM_F_LINEAR, xbcs);
    print_attempt("Top alias", ROM_TOP_LINEAR, xbcs);
    return 0;
}
