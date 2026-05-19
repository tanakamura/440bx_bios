#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path
import argparse
import struct
import sys

import gen_blob


ROM_SIZE = 256 * 1024
ROM_LOW_BASE = 0x000C0000
ROM_HIGH_BASE = 0x100000000 - ROM_SIZE
DIRECTORY_BYTES = 0x2000
PAYLOAD_ALIGN = 16

ROM_MAGIC = 0x304D5242  # 'BRM0'
ROM_VERSION = 1
ROM_HEADER_SIZE = 32
ROM_ENTRY_SIZE = 32

PAYLOAD_IDS = {
    "stage2": 1,
    "stage3": 2,
    "legacy": 3,
    "linux_loader": 4,
    "vgabios": 5,
    "dsdt": 6,
    "test_elf": 7,
    "selftest": 8,
    "test_floppy": 9,
}

PAYLOAD_TYPE_BLZ4 = 1
PAYLOAD_TYPE_APP = 2
PAYLOAD_TYPE_RAW = 3


def align_up(value: int, align: int) -> int:
    return (value + align - 1) & ~(align - 1)


def read_u16(data: bytes, off: int) -> int:
    return struct.unpack_from("<H", data, off)[0]


def read_u32(data: bytes, off: int) -> int:
    return struct.unpack_from("<I", data, off)[0]


def is_elf32(data: bytes) -> bool:
    return data.startswith(b"\x7fELF") and len(data) >= 52 and data[4] == 1


def extract_elf_load_image(data: bytes) -> tuple[bytes, int]:
    if not is_elf32(data):
        raise ValueError("not an ELF32 file")
    if data[5] != 1:
        raise ValueError("only little-endian ELF32 is supported")

    phoff = read_u32(data, 28)
    phentsize = read_u16(data, 42)
    phnum = read_u16(data, 44)
    loads: list[tuple[int, int, int, int]] = []

    for i in range(phnum):
        off = phoff + i * phentsize
        if off + 32 > len(data):
            raise ValueError("ELF program header is truncated")
        p_type = read_u32(data, off)
        if p_type != 1:  # PT_LOAD
            continue
        p_offset = read_u32(data, off + 4)
        p_vaddr = read_u32(data, off + 8)
        p_paddr = read_u32(data, off + 12)
        p_filesz = read_u32(data, off + 16)
        p_memsz = read_u32(data, off + 20)
        addr = p_paddr if p_paddr != 0 else p_vaddr
        if p_filesz > p_memsz or p_offset + p_filesz > len(data):
            raise ValueError("bad ELF PT_LOAD bounds")
        loads.append((addr, p_offset, p_filesz, p_memsz))

    if not loads:
        raise ValueError("ELF has no PT_LOAD segment")

    base = min(addr for addr, _, _, _ in loads)
    end = max(addr + memsz for addr, _, _, memsz in loads)
    image = bytearray(end - base)
    for addr, p_offset, p_filesz, _ in loads:
        dst = addr - base
        image[dst:dst + p_filesz] = data[p_offset:p_offset + p_filesz]
    return bytes(image), base


def read_payload(path: Path) -> tuple[bytes, int | None]:
    data = path.read_bytes()
    if is_elf32(data):
        image, load_addr = extract_elf_load_image(data)
        return image, load_addr
    return data, None


def resolve_payload_path(list_path: Path, name: str) -> Path:
    path = Path(name)
    if path.is_absolute():
        return path
    candidates = [list_path.parent / path, Path.cwd() / path]
    for candidate in candidates:
        if candidate.exists():
            return candidate
    return candidates[0]


def parse_blob_list(path: Path) -> list[tuple[str, Path]]:
    entries: list[tuple[str, Path]] = []
    for lineno, raw_line in enumerate(path.read_text().splitlines(), 1):
        line = raw_line.split("#", 1)[0].strip()
        if not line:
            continue
        fields = [field.strip() for field in line.split(",", 1)]
        if len(fields) != 2 or not fields[0] or not fields[1]:
            raise ValueError(f"{path}:{lineno}: expected '<type>,<filename>'")
        payload_type, filename = fields
        if payload_type != "stage1" and payload_type not in PAYLOAD_IDS:
            raise ValueError(f"{path}:{lineno}: unknown payload type {payload_type}")
        entries.append((payload_type, resolve_payload_path(path, filename)))
    return entries


def write_payload(rom: bytearray, cursor: int, payload: bytes) -> int:
    cursor = align_up(cursor, PAYLOAD_ALIGN)
    end = cursor + len(payload)
    if end > ROM_SIZE - 8:
        raise ValueError("ROM payload area overflow")
    rom[cursor:end] = payload
    return cursor


def build_rom(entries: list[tuple[str, Path]]) -> bytes:
    rom = bytearray([0xFF] * ROM_SIZE)
    payload_entries = []
    payload_cursor = DIRECTORY_BYTES
    stage1_start = ROM_SIZE - 8

    stage1_items = [(kind, path) for kind, path in entries if kind == "stage1"]
    if len(stage1_items) != 1:
        raise ValueError("blob list must contain exactly one stage1 entry")

    for kind, path in entries:
        data, load_addr = read_payload(path)
        if kind == "stage1":
            if load_addr is not None and ROM_LOW_BASE <= load_addr < 0x00100000:
                off = load_addr - ROM_LOW_BASE
            else:
                off = ROM_SIZE - len(data)
            if off < DIRECTORY_BYTES or off + len(data) > ROM_SIZE:
                raise ValueError("stage1 does not fit at the end of ROM")
            rom[off:off + len(data)] = data
            stage1_start = min(stage1_start, off)
            continue

        payload_id = PAYLOAD_IDS[kind]
        payload_type = PAYLOAD_TYPE_RAW if kind == "test_floppy" else PAYLOAD_TYPE_BLZ4
        if payload_type == PAYLOAD_TYPE_BLZ4:
            payload, _, _ = gen_blob.make_blob(data, load_addr)
        else:
            payload = data

        payload_off = write_payload(rom, payload_cursor, payload)
        payload_cursor = payload_off + len(payload)
        if payload_cursor > stage1_start:
            raise ValueError("payloads overlap stage1")
        payload_entries.append(
            (
                payload_id,
                payload_type,
                0,
                payload_off,
                len(payload),
                len(payload),
                0 if load_addr is None else load_addr,
                0,
            )
        )

    if ROM_HEADER_SIZE + len(payload_entries) * ROM_ENTRY_SIZE > DIRECTORY_BYTES:
        raise ValueError("payload directory overflow")

    struct.pack_into(
        "<IIIIIIII",
        rom,
        0,
        ROM_MAGIC,
        ROM_VERSION,
        ROM_HEADER_SIZE,
        ROM_ENTRY_SIZE,
        len(payload_entries),
        DIRECTORY_BYTES,
        payload_cursor,
        stage1_start,
    )

    entry_off = ROM_HEADER_SIZE
    for entry in payload_entries:
        struct.pack_into("<IIIIIIII", rom, entry_off, *entry)
        entry_off += ROM_ENTRY_SIZE

    free_first = ROM_HIGH_BASE + align_up(payload_cursor, PAYLOAD_ALIGN)
    free_end = ROM_HIGH_BASE + stage1_start
    if free_first > free_end:
        free_first = free_end
    struct.pack_into("<II", rom, ROM_SIZE - 8, free_first, free_end)
    return bytes(rom)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("blob_list", type=Path)
    parser.add_argument("dst", type=Path)
    args = parser.parse_args()

    try:
        entries = parse_blob_list(args.blob_list)
        rom = build_rom(entries)
    except ValueError as exc:
        print(f"gen_rom.py: {exc}", file=sys.stderr)
        return 1

    args.dst.write_bytes(rom)
    print(f"wrote {args.dst} size={len(rom)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
