#include <conio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    unsigned char code = 42u;

    if (argc > 1) {
        code = (unsigned char)strtoul(argv[1], 0, 0);
    }

    outp(0x00f4, code);
    for (;;) {
    }
}
