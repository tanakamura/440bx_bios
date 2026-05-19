#include <dos.h>
#include <stdio.h>
#include <string.h>

static int failures;

static void report(const char *name, int ok)
{
    printf("TEST %s %s\r\n", name, ok ? "OK" : "NG");
    if (!ok) {
        ++failures;
    }
}

static int read_sector(unsigned short cyl, unsigned short head,
                       unsigned short sector, void far *buffer)
{
    union REGS inregs;
    union REGS outregs;
    struct SREGS sregs;

    memset(&inregs, 0, sizeof(inregs));
    memset(&sregs, 0, sizeof(sregs));
    inregs.h.ah = 0x02;
    inregs.h.al = 0x01;
    inregs.h.ch = (unsigned char)cyl;
    inregs.h.cl = (unsigned char)((sector & 0x3f) | ((cyl >> 2) & 0xc0));
    inregs.h.dh = (unsigned char)head;
    inregs.h.dl = 0x00;
    sregs.es = FP_SEG(buffer);
    inregs.x.bx = FP_OFF(buffer);
    int86x(0x13, &inregs, &outregs, &sregs);
    return outregs.x.cflag == 0;
}

static void test_int11(void)
{
    union REGS inregs;
    union REGS outregs;

    memset(&inregs, 0, sizeof(inregs));
    int86(0x11, &inregs, &outregs);
    report("int11_floppy", (outregs.x.ax & 0x0001u) != 0);
}

static void test_int12(void)
{
    union REGS inregs;
    union REGS outregs;

    memset(&inregs, 0, sizeof(inregs));
    int86(0x12, &inregs, &outregs);
    report("int12_basekb", outregs.x.ax >= 128u && outregs.x.ax <= 640u);
}

static void test_int13_08(void)
{
    union REGS inregs;
    union REGS outregs;
    unsigned short spt;
    unsigned short heads;

    memset(&inregs, 0, sizeof(inregs));
    inregs.h.ah = 0x08;
    inregs.h.dl = 0x00;
    int86(0x13, &inregs, &outregs);
    spt = outregs.x.cx & 0x003fu;
    heads = ((outregs.x.dx >> 8) & 0x00ffu) + 1u;
    report("int13_08", outregs.x.cflag == 0 &&
                       outregs.h.bl != 0 &&
                       spt >= 9u &&
                       heads >= 1u);
}

static void test_int13_15(void)
{
    union REGS inregs;
    union REGS outregs;

    memset(&inregs, 0, sizeof(inregs));
    inregs.h.ah = 0x15;
    inregs.h.dl = 0x00;
    int86(0x13, &inregs, &outregs);
    report("int13_15", outregs.x.cflag == 0 && outregs.h.ah == 0x01);
}

static void test_boot_sector(void)
{
    static unsigned char sector[512];
    int ok;

    memset(sector, 0, sizeof(sector));
    ok = read_sector(0, 0, 1, sector);
    ok = ok &&
         sector[0] == 0xeb &&
         sector[1] == 0x3c &&
         sector[2] == 0x90 &&
         sector[11] == 0x00 &&
         sector[12] == 0x02 &&
         sector[19] == 0x40 &&
         sector[20] == 0x0b;
    report("int13_bootsec", ok);
}

static void test_root_dir(void)
{
    static unsigned char sector[512];
    int ok;
    unsigned int i;

    memset(sector, 0, sizeof(sector));
    ok = read_sector(0, 1, 2, sector);
    if (ok) {
        ok = 0;
        for (i = 0; i + 11 <= sizeof(sector); ++i) {
            if (memcmp(sector + i, "KERNEL   SYS", 11) == 0 ||
                memcmp(sector + i, "COMMAND COM", 11) == 0) {
                ok = 1;
                break;
            }
        }
    }
    report("int13_rootdir", ok);
}

static void test_int1a_00(void)
{
    union REGS inregs;
    union REGS outregs;

    memset(&inregs, 0, sizeof(inregs));
    inregs.h.ah = 0x00;
    int86(0x1a, &inregs, &outregs);
    report("int1a_00", outregs.x.cflag == 0);
}

static void test_int1a_02(void)
{
    union REGS inregs;
    union REGS outregs;
    unsigned char sec;
    unsigned char min;
    unsigned char hour;

    memset(&inregs, 0, sizeof(inregs));
    inregs.h.ah = 0x02;
    int86(0x1a, &inregs, &outregs);
    sec = outregs.h.dh;
    min = outregs.h.cl;
    hour = outregs.h.ch;
    report("int1a_02", outregs.x.cflag == 0 &&
                       sec <= 0x59 &&
                       min <= 0x59 &&
                       hour <= 0x23);
}

int main(void)
{
    failures = 0;
    puts("BIOSTEST START");
    test_int11();
    test_int12();
    test_int13_08();
    test_int13_15();
    test_boot_sector();
    test_root_dir();
    test_int1a_00();
    test_int1a_02();
    if (failures == 0) {
        puts("TEST SUMMARY OK");
    } else {
        printf("TEST SUMMARY NG %d\r\n", failures);
    }
    return failures;
}
