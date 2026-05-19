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

SHF_ALLOC = 0x2
SHT_PROGBITS = 1

STAGE1_EXCLUDED_SECTIONS = {
    ".rom_anchor",
    ".rom_free_descriptor",
    ".biosblob",
    ".stage2blob",
    ".dsdtblob",
    ".vgabiosblob",
    ".testelfblob",
}


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


def read_c_string(data: bytes, off: int) -> str:
    end = data.find(b"\0", off)
    if end < 0:
        end = len(data)
    return data[off:end].decode("ascii", "replace")


def extract_stage1_overlays(data: bytes) -> list[tuple[int, bytes]]:
    if not is_elf32(data):
        raise ValueError("not an ELF32 file")
    if data[5] != 1:
        raise ValueError("only little-endian ELF32 is supported")

    shoff = read_u32(data, 32)
    shentsize = read_u16(data, 46)
    shnum = read_u16(data, 48)
    shstrndx = read_u16(data, 50)
    if shentsize < 40 or shstrndx >= shnum:
        raise ValueError("bad ELF section table")

    shstr_off = shoff + shstrndx * shentsize
    if shstr_off + 40 > len(data):
        raise ValueError("ELF section string table header is truncated")
    strtab_off = read_u32(data, shstr_off + 16)
    strtab_size = read_u32(data, shstr_off + 20)
    if strtab_off + strtab_size > len(data):
        raise ValueError("ELF section string table is truncated")
    strtab = data[strtab_off:strtab_off + strtab_size]

    overlays: list[tuple[int, bytes]] = []
    for i in range(shnum):
        off = shoff + i * shentsize
        if off + 40 > len(data):
            raise ValueError("ELF section header is truncated")
        name_off = read_u32(data, off)
        sh_type = read_u32(data, off + 4)
        sh_flags = read_u32(data, off + 8)
        sh_addr = read_u32(data, off + 12)
        sh_offset = read_u32(data, off + 16)
        sh_size = read_u32(data, off + 20)
        name = read_c_string(strtab, name_off) if name_off < len(strtab) else ""

        if (sh_flags & SHF_ALLOC) == 0 or sh_type != SHT_PROGBITS:
            continue
        if sh_size == 0 or name in STAGE1_EXCLUDED_SECTIONS:
            continue
        if sh_addr < ROM_LOW_BASE or sh_addr >= ROM_LOW_BASE + ROM_SIZE:
            continue
        if sh_offset + sh_size > len(data):
            raise ValueError(f"stage1 section {name} is truncated")
        rom_off = sh_addr - ROM_LOW_BASE
        if rom_off + sh_size > ROM_SIZE:
            raise ValueError(f"stage1 section {name} exceeds ROM")
        overlays.append((rom_off, data[sh_offset:sh_offset + sh_size]))

    if not overlays:
        raise ValueError("stage1 ELF has no ROM overlay sections")
    return overlays


def read_payload(path: Path) -> tuple[bytes, int | None]:
    data = path.read_bytes()
    if is_elf32(data):
        image, load_addr = extract_elf_load_image(data)
        return image, load_addr
    return data, None


def read_stage1_overlays(path: Path) -> list[tuple[int, bytes]]:
    data = path.read_bytes()
    if is_elf32(data):
        return extract_stage1_overlays(data)
    if len(data) > ROM_SIZE - DIRECTORY_BYTES - 8:
        raise ValueError("raw stage1 image is too large")
    return [(ROM_SIZE - 8 - len(data), data)]


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
        if kind == "stage1":
            for off, data in read_stage1_overlays(path):
                if off < DIRECTORY_BYTES or off + len(data) > ROM_SIZE - 8:
                    raise ValueError("stage1 does not fit at the end of ROM")
                rom[off:off + len(data)] = data
                stage1_start = min(stage1_start, off)
            continue

        data, load_addr = read_payload(path)
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
