#!/usr/bin/env python3
from pathlib import Path
import argparse
import struct


U32 = 1 << 32


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("rom", type=Path)
    parser.add_argument("payload", type=Path)
    args = parser.parse_args()

    rom = bytearray(args.rom.read_bytes())
    payload = args.payload.read_bytes()
    if len(rom) < 8:
        raise SystemExit("ROM is too small")

    high_base = U32 - len(rom)
    free_first, free_end = struct.unpack_from("<II", rom, len(rom) - 8)
    if not (high_base <= free_first <= free_end <= U32 - 8):
        raise SystemExit(
            f"bad ROM free descriptor first=0x{free_first:08x} "
            f"end=0x{free_end:08x} high_base=0x{high_base:08x}"
        )

    first_off = free_first - high_base
    end_off = free_end - high_base
    capacity = end_off - first_off
    if len(payload) > capacity:
        raise SystemExit(
            f"payload too large: {len(payload)} > free capacity {capacity}"
        )

    rom[first_off:first_off + len(payload)] = payload
    args.rom.write_bytes(rom)
    print(
        f"patched {args.rom} free=0x{free_first:08x}-0x{free_end:08x} "
        f"off=0x{first_off:05x} size={len(payload)} capacity={capacity}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
