#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path
import argparse
import zlib


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("binary", type=Path)
    parser.add_argument("out", type=Path)
    parser.add_argument("--prefix", default="STAGE15_BLOB")
    args = parser.parse_args()

    data = args.binary.read_bytes()
    crc = zlib.crc32(data) & 0xFFFFFFFF
    args.out.write_text(
        f"; generated from {args.binary}\n"
        f"%define {args.prefix}_SIZE 0x{len(data):x}\n"
        f"%define {args.prefix}_CRC32 0x{crc:08x}\n"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
