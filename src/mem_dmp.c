#include <dos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int bios_flat_call(unsigned char op, unsigned long addr,
                          unsigned char *value)
{
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

static int bios_flat_read8(unsigned long addr, unsigned char *value)
{
    return bios_flat_call(0x00u, addr, value);
}

static int bios_flat_write8(unsigned long addr, unsigned char value)
{
    return bios_flat_call(0x01u, addr, &value);
}

static void print_usage(void)
{
    puts("usage:");
    puts("  MEM_DMP r <addr> <length>");
    puts("  MEM_DMP f <addr> <length> <val>");
    puts("unit: 8 bytes");
}

static unsigned long parse_u32(const char *s)
{
    return strtoul(s, 0, 0);
}

static int dump_region(unsigned long addr, unsigned long length)
{
    unsigned long end;
    unsigned long p;

    end = addr + length;
    for (p = addr; p < end; p += 8UL) {
        unsigned int i;
        printf("%08lX:", p);
        for (i = 0; i < 8; ++i) {
            unsigned long cur = p + (unsigned long)i;
            if (cur < end) {
                unsigned char value = 0xffu;
                if (bios_flat_read8(cur, &value) != 0) {
                    printf(" ??");
                } else {
                    printf(" %02X", value);
                }
            } else {
                printf("   ");
            }
        }
        printf("\r\n");
    }
    return 0;
}

static int fill_region(unsigned long addr, unsigned long length,
                       unsigned char value)
{
    unsigned long end;
    unsigned long p;

    end = addr + length;
    for (p = addr; p < end; ++p) {
        if (bios_flat_write8(p, value) != 0) {
            printf("write failed @ %08lX\r\n", p);
            return 1;
        }
    }
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 4) {
        print_usage();
        return 1;
    }

    if ((argv[1][0] == 'r' || argv[1][0] == 'R') && argv[1][1] == '\0') {
        unsigned long addr = parse_u32(argv[2]);
        unsigned long length = parse_u32(argv[3]);
        return dump_region(addr, length);
    }

    if ((argv[1][0] == 'f' || argv[1][0] == 'F') &&
        argv[1][1] == '\0' &&
        argc >= 5) {
        unsigned long addr = parse_u32(argv[2]);
        unsigned long length = parse_u32(argv[3]);
        unsigned char value = (unsigned char)parse_u32(argv[4]);
        return fill_region(addr, length, value);
    }

    print_usage();
    return 1;
}
