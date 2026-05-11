#include <dos.h>
#include <stdio.h>
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

int main(void)
{
    static unsigned char buf[16];
    unsigned long linear;
    unsigned char value;

    linear = ((unsigned long)FP_SEG(buf) << 4) + (unsigned long)FP_OFF(buf);
    puts("FLATTEST START");
    buf[0] = 0x12u;
    if (bios_flat_read8(linear, &value) != 0 || value != 0x12u) {
        puts("TEST int60_flat NG");
        return 1;
    }
    if (bios_flat_write8(linear, 0x34u) != 0) {
        puts("TEST int60_flat NG");
        return 1;
    }
    if (bios_flat_read8(linear, &value) != 0 || value != 0x34u || buf[0] != 0x34u) {
        puts("TEST int60_flat NG");
        return 1;
    }
    puts("TEST int60_flat OK");
    return 0;
}
