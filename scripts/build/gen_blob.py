#!/usr/bin/env python3
from pathlib import Path
import argparse
import binascii
import re
import struct


SECTOR = 512
MINMATCH = 4
MAX_OFFSET = 0xFFFF
REPO_ROOT = Path(__file__).resolve().parents[2]
SRC_DIR = REPO_ROOT / "src"

BLOB_DEFINE_NAMES = {
    "BLOB_MAGIC",
    "BLOB_VERSION",
    "BLOB_FLAG_LZ4_BLOCKS",
    "BLOB_FLAG_HAS_LOAD_ADDR",
    "BLOB_BLOCK_SIZE",
    "BLOB_HEADER_SIZE",
    "BLOB_BLOCK_DESC_SIZE",
}


def parse_numeric_defines(path: Path) -> dict[str, int]:
    pattern = re.compile(r"^#define\s+([A-Z0-9_]+)\s+(.+?)\s*$")
    defines: dict[str, int] = {}
    for raw_line in path.read_text().splitlines():
        match = pattern.match(raw_line.strip())
        if not match:
            continue
        name, value = match.groups()
        if name not in BLOB_DEFINE_NAMES:
            continue
        value = value.split("/*", 1)[0].strip()
        value = re.sub(r"[uUlL]+$", "", value)
        if not re.fullmatch(r"0x[0-9a-fA-F]+|[0-9]+", value):
            raise ValueError(f"unsupported define value for {name}: {value}")
        defines[name] = int(value, 0)
    missing = sorted(BLOB_DEFINE_NAMES - set(defines))
    if missing:
        raise ValueError(f"missing defines: {', '.join(missing)}")
    return defines


BLOB_DEFINES = parse_numeric_defines(SRC_DIR / "include" / "blob.h")
MAGIC = BLOB_DEFINES["BLOB_MAGIC"]
VERSION = BLOB_DEFINES["BLOB_VERSION"]
FLAG_LZ4_BLOCKS = BLOB_DEFINES["BLOB_FLAG_LZ4_BLOCKS"]
FLAG_HAS_LOAD_ADDR = BLOB_DEFINES["BLOB_FLAG_HAS_LOAD_ADDR"]
BLOCK_SIZE = BLOB_DEFINES["BLOB_BLOCK_SIZE"]
HEADER_SIZE = BLOB_DEFINES["BLOB_HEADER_SIZE"]
BLOCK_DESC_SIZE = BLOB_DEFINES["BLOB_BLOCK_DESC_SIZE"]


def le16(buf: bytes, off: int) -> int:
    return buf[off] | (buf[off + 1] << 8)


def fat12_get(fat: bytes, cluster: int) -> int:
    pos = (cluster * 3) // 2
    if (cluster & 1) == 0:
        return fat[pos] | ((fat[pos + 1] & 0x0F) << 8)
    return ((fat[pos] >> 4) | (fat[pos + 1] << 4)) & 0x0FFF


def collect_used_fat12_sectors(data: bytes) -> list[int]:
    boot = data[:SECTOR]
    bytes_per_sector = le16(boot, 0x0B)
    sectors_per_cluster = boot[0x0D]
    reserved_sectors = le16(boot, 0x0E)
    fat_count = boot[0x10]
    root_entries = le16(boot, 0x11)
    sectors_per_fat = le16(boot, 0x16)

    if bytes_per_sector != SECTOR or sectors_per_cluster == 0 or fat_count == 0:
        raise ValueError("unsupported BPB")

    root_dir_sectors = (root_entries * 32 + (SECTOR - 1)) // SECTOR
    fat_lba = reserved_sectors
    root_lba = fat_lba + fat_count * sectors_per_fat
    data_lba = root_lba + root_dir_sectors

    used = set()
    used.update(range(0, reserved_sectors))
    used.update(range(fat_lba, fat_lba + fat_count * sectors_per_fat))
    used.update(range(root_lba, root_lba + root_dir_sectors))

    fat = data[fat_lba * SECTOR:(fat_lba + sectors_per_fat) * SECTOR]
    root = data[root_lba * SECTOR:(root_lba + root_dir_sectors) * SECTOR]

    def add_chain(start_cluster: int) -> None:
        seen = set()
        cluster = start_cluster
        while 2 <= cluster < 0xFF8 and cluster not in seen:
            seen.add(cluster)
            lba = data_lba + (cluster - 2) * sectors_per_cluster
            for s in range(sectors_per_cluster):
                used.add(lba + s)
            cluster = fat12_get(fat, cluster)

    for off in range(0, len(root), 32):
        ent = root[off:off + 32]
        first = ent[0]
        attr = ent[11]
        if first == 0x00:
            break
        if first == 0xE5 or attr == 0x0F:
            continue
        start_cluster = le16(ent, 26)
        if start_cluster >= 2:
            add_chain(start_cluster)

    sectors = len(data) // SECTOR
    return sorted(n for n in used if n < sectors)


def normalize_fat12(data: bytes) -> bytes:
    if len(data) % SECTOR != 0:
        raise ValueError("image size is not sector aligned")
    used = collect_used_fat12_sectors(data)
    out = bytearray(len(data))
    for lba in used:
        start = lba * SECTOR
        out[start:start + SECTOR] = data[start:start + SECTOR]
    return bytes(out)


def emit_len(out: bytearray, length: int) -> int:
    token_part = min(length, 15)
    if length >= 15:
        length -= 15
        while length >= 255:
            out.append(255)
            length -= 255
        out.append(length)
    return token_part


def lz4_emit_sequence(out: bytearray, literals: bytes, offset: int | None,
                      match_len: int) -> None:
    token_pos = len(out)
    out.append(0)
    lit_token = emit_len(out, len(literals))
    out[token_pos] = lit_token << 4
    out.extend(literals)

    if offset is None:
        return

    out.extend(struct.pack("<H", offset))
    ml = match_len - MINMATCH
    ml_token = emit_len(out, ml)
    out[token_pos] |= ml_token


def lz4_compress_block(data: bytes) -> bytes:
    n = len(data)
    out = bytearray()
    table: dict[int, int] = {}
    anchor = 0
    i = 0

    while i + MINMATCH <= n:
        key = struct.unpack_from("<I", data, i)[0]
        ref = table.get(key)
        table[key] = i

        if ref is not None and i - ref <= MAX_OFFSET and data[ref:ref + 4] == data[i:i + 4]:
            match_len = MINMATCH
            max_len = n - i
            while match_len < max_len and data[ref + match_len] == data[i + match_len]:
                match_len += 1

            lz4_emit_sequence(out, data[anchor:i], i - ref, match_len)

            end = i + match_len
            j = i + 1
            while j + MINMATCH <= n and j < end:
                table[struct.unpack_from("<I", data, j)[0]] = j
                j += 1
            i = end
            anchor = i
        else:
            i += 1

    lz4_emit_sequence(out, data[anchor:], None, 0)
    return bytes(out)


def lz4_read_len(src: bytes, ip: int, base: int) -> tuple[int, int]:
    length = base
    if base != 15:
        return length, ip
    while True:
        if ip >= len(src):
            raise ValueError("truncated length")
        value = src[ip]
        ip += 1
        length += value
        if value != 255:
            return length, ip


def lz4_decompress_block(src: bytes, output_size: int) -> bytes:
    out = bytearray()
    ip = 0
    while ip < len(src):
        token = src[ip]
        ip += 1
        lit_len, ip = lz4_read_len(src, ip, token >> 4)
        if ip + lit_len > len(src):
            raise ValueError("literal overrun")
        out.extend(src[ip:ip + lit_len])
        ip += lit_len
        if ip == len(src):
            break
        if ip + 2 > len(src):
            raise ValueError("missing offset")
        offset = src[ip] | (src[ip + 1] << 8)
        ip += 2
        if offset == 0 or offset > len(out):
            raise ValueError("bad offset")
        match_len, ip = lz4_read_len(src, ip, token & 0x0F)
        match_len += MINMATCH
        for _ in range(match_len):
            out.append(out[-offset])
    if len(out) != output_size:
        raise ValueError(f"output size {len(out)} != {output_size}")
    return bytes(out)


def crc32(data: bytes) -> int:
    return binascii.crc32(data) & 0xFFFFFFFF


def make_blob(payload: bytes, load_addr: int | None = None) -> tuple[bytes, int, int]:
    blocks = []
    compressed_payload = bytearray()
    block_count = (len(payload) + BLOCK_SIZE - 1) // BLOCK_SIZE
    block_table_off = HEADER_SIZE
    data_off = block_table_off + BLOCK_DESC_SIZE * block_count
    flags = FLAG_LZ4_BLOCKS
    if load_addr is not None:
        flags |= FLAG_HAS_LOAD_ADDR

    for block_index in range(block_count):
        uncompressed_off = block_index * BLOCK_SIZE
        chunk = payload[uncompressed_off:uncompressed_off + BLOCK_SIZE]
        compressed = lz4_compress_block(chunk)
        decoded = lz4_decompress_block(compressed, len(chunk))
        if decoded != chunk:
            raise ValueError("internal lz4 block roundtrip failed")
        compressed_off = len(compressed_payload)
        compressed_payload += compressed
        blocks.append(
            (
                uncompressed_off,
                len(chunk),
                compressed_off,
                len(compressed),
                crc32(compressed),
            )
        )

    out = bytearray()
    out += struct.pack(
        "<IIIIIIIIIIIII",
        MAGIC,
        HEADER_SIZE,
        VERSION,
        flags,
        0 if load_addr is None else load_addr,
        len(payload),
        len(compressed_payload),
        BLOCK_SIZE,
        block_count,
        block_table_off,
        data_off,
        crc32(payload),
        crc32(compressed_payload),
    )
    for block in blocks:
        out += struct.pack("<IIIII", *block)
    out += compressed_payload
    return bytes(out), len(compressed_payload), block_count


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--fat12-normalize", action="store_true")
    parser.add_argument("--load-addr", type=lambda value: int(value, 0))
    parser.add_argument("src", type=Path)
    parser.add_argument("dst", type=Path)
    args = parser.parse_args()

    payload = args.src.read_bytes()
    if args.fat12_normalize:
        payload = normalize_fat12(payload)

    blob, compressed_size, block_count = make_blob(payload, args.load_addr)
    args.dst.write_bytes(blob)
    print(
        f"wrote {args.dst} raw={len(payload)} blob={len(blob)} "
        f"compressed={compressed_size} blocks={block_count}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
