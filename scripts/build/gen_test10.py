#!/usr/bin/env python3
from pathlib import Path


SECTOR = 512
TOTAL = 1440 * 1024


def main() -> int:
    boot = bytearray(SECTOR)

    code = bytes(
        [
            0xFA,                    # cli
            0x31, 0xC0,              # xor ax,ax
            0x8E, 0xD0,              # mov ss,ax
            0xBC, 0x00, 0x7C,        # mov sp,7c00
            0xB0, 0x54,              # mov al,'T'
            0xB4, 0x0E,              # mov ah,0e
            0xCD, 0x10,              # int 10
            0xEB, 0xFE,              # jmp $
        ]
    )
    boot[: len(code)] = code
    boot[510:512] = b"\x55\xAA"

    image = bytearray(TOTAL)
    image[0:SECTOR] = boot

    Path("test10.img").write_bytes(image)
    print("wrote test10.img")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
