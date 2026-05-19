#!/usr/bin/env python3
from pathlib import Path
import struct


ROOT = Path.cwd()
SECTOR = 512
DISK_SECTORS = 32768
PART1_START = 2048
PART2_START = 4096
PART2_SECTORS = 8


def put_part(mbr: bytearray, index: int, boot: int, ptype: int,
             start: int, sectors: int) -> None:
    off = 0x1BE + index * 16
    mbr[off + 0] = boot
    mbr[off + 1:off + 4] = b"\x00\x02\x00"
    mbr[off + 4] = ptype
    mbr[off + 5:off + 8] = b"\x00\x02\x00"
    struct.pack_into("<I", mbr, off + 8, start)
    struct.pack_into("<I", mbr, off + 12, sectors)


def main() -> None:
    elf = (ROOT / "linuxprobe.elf").read_bytes()
    part1_sectors = (len(elf) + SECTOR - 1) // SECTOR
    if PART1_START + part1_sectors > PART2_START:
        raise SystemExit("linuxprobe.elf is too large")

    image = bytearray(DISK_SECTORS * SECTOR)
    put_part(image, 0, 0x80, 0x83, PART1_START, part1_sectors)
    put_part(image, 1, 0x00, 0x83, PART2_START, PART2_SECTORS)
    image[0x1FE:0x200] = b"\x55\xAA"
    image[PART1_START * SECTOR:PART1_START * SECTOR + len(elf)] = elf
    initrd = b"INITRDTEST\n"
    off = PART2_START * SECTOR
    image[off:off + len(initrd)] = initrd
    (ROOT / "linuxprobe.img").write_bytes(image)

    raw_image = bytearray(DISK_SECTORS * SECTOR)
    raw_image[:len(elf)] = elf
    (ROOT / "linuxprobe_raw.img").write_bytes(raw_image)


if __name__ == "__main__":
    main()
