%include "tests/boot/linuxprobe.asm"

section .defer progbits alloc
align 16
defer_marker:
    times 512 db 0x5a
