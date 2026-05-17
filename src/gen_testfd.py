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
            0x8E, 0xD8,              # mov ds,ax
            0x8E, 0xD0,              # mov ss,ax
            0xBC, 0x00, 0x7C,        # mov sp,7c00
            0x8E, 0xC0,              # mov es,ax
            0xBB, 0x00, 0x80,        # mov bx,8000
            0xB8, 0x01, 0x02,        # mov ax,0201
            0xB9, 0x02, 0x00,        # mov cx,0002
            0x31, 0xD2,              # xor dx,dx
            0xCD, 0x13,              # int 13
            0x72, 0x16,              # jc fail
            0xB0, 0x53,              # mov al,'S'
            0xB4, 0x0E,              # mov ah,0e
            0xCD, 0x10,              # int 10
            0xA0, 0x00, 0x80,        # mov al,[8000]
            0xB4, 0x0E,              # mov ah,0e
            0xCD, 0x10,              # int 10
            0xBA, 0xF4, 0x00,        # mov dx,00f4
            0xB8, 0x2A, 0x00,        # mov ax,002a
            0xEF,                    # out dx,ax
            0xEB, 0xFE,              # jmp $
            0xB0, 0x21,              # mov al,'!'
            0xB4, 0x0E,              # mov ah,0e
            0xCD, 0x10,              # int 10
            0xBA, 0xF4, 0x00,        # mov dx,00f4
            0xB8, 0x2B, 0x00,        # mov ax,002b
            0xEF,                    # out dx,ax
            0xEB, 0xFE,              # jmp $
        ]
    )
    boot[: len(code)] = code
    boot[510:512] = b"\x55\xAA"

    image = bytearray(TOTAL)
    image[0:SECTOR] = boot
    image[SECTOR] = ord("Q")

    Path("testfd.img").write_bytes(image)
    print("wrote testfd.img")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
