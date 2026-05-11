#!/usr/bin/env python3
from pathlib import Path
import binascii
import struct
import sys


SECTOR = 512
CRC_BLOCK = 1024
MAGIC = 0x30445346  # 'FDS0'


def le16(buf: bytes, off: int) -> int:
    return buf[off] | (buf[off + 1] << 8)


def fat12_get(fat: bytes, cluster: int) -> int:
    pos = (cluster * 3) // 2
    if (cluster & 1) == 0:
        return fat[pos] | ((fat[pos + 1] & 0x0F) << 8)
    return ((fat[pos] >> 4) | (fat[pos + 1] << 4)) & 0x0FFF


def collect_used_sectors(data: bytes) -> list[int]:
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

    return sorted(used)


def main() -> int:
    if len(sys.argv) >= 3:
        src = Path(sys.argv[1])
        dst = Path(sys.argv[2])
    else:
        src = Path("../freedos_boot_fd.img")
        dst = Path("fdos_sparse.bin")
    data = src.read_bytes()
    if len(data) % SECTOR != 0:
        raise SystemExit("image size is not sector aligned")
    sectors = len(data) // SECTOR

    try:
        nonzero = collect_used_sectors(data)
    except ValueError:
        nonzero = []
        for i in range(sectors):
            blk = data[i * SECTOR:(i + 1) * SECTOR]
            if any(blk):
                nonzero.append(i)

    normalized = bytearray(len(data))
    for i in nonzero:
        normalized[i * SECTOR:(i + 1) * SECTOR] = data[i * SECTOR:(i + 1) * SECTOR]
    data = bytes(normalized)

    runs = []
    if nonzero:
        start = prev = nonzero[0]
        for n in nonzero[1:]:
            if n == prev + 1:
                prev = n
                continue
            runs.append((start, prev - start + 1))
            start = prev = n
        runs.append((start, prev - start + 1))

    crc_count = (len(data) + CRC_BLOCK - 1) // CRC_BLOCK
    header_size = 32
    run_size = 8
    crc_table_off = header_size + run_size * len(runs)
    data_off = crc_table_off + 4 * crc_count
    image_crc32 = binascii.crc32(data) & 0xffffffff

    out = bytearray()
    out += struct.pack("<IHHIIIIII", MAGIC, len(runs), 0, sectors, CRC_BLOCK,
                       crc_count, crc_table_off, data_off, image_crc32)

    payload = bytearray()
    payload_off = data_off
    for lba, count in runs:
        out += struct.pack("<HHI", lba, count, payload_off)
        chunk = data[lba * SECTOR:(lba + count) * SECTOR]
        payload += chunk
        payload_off += len(chunk)

    for i in range(crc_count):
        blk = data[i * CRC_BLOCK:(i + 1) * CRC_BLOCK]
        out += struct.pack("<I", binascii.crc32(blk) & 0xffffffff)

    out += payload
    dst.write_bytes(out)
    print(f"wrote {dst} size={len(out)} runs={len(runs)} sectors={sectors}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
