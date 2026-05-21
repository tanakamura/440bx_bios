# BLZ4 blob format

The canonical C definition is `src/include/blob.h`. Build tools must derive
numeric ABI constants from that header instead of duplicating them.

All fields are little-endian 32-bit values.

```
offset  size  name
0x00    4     magic = "BLZ4" (0x345a4c42)
0x04    4     header_size = 52
0x08    4     version = 3
0x0c    4     flags
0x10    4     load_addr
0x14    4     uncompressed_size
0x18    4     compressed_size
0x1c    4     block_size = 4096
0x20    4     block_count
0x24    4     block_table_off
0x28    4     data_off
0x2c    4     uncompressed_crc32
0x30    4     compressed_crc32
```

Defined flags:

```
0x00000002  BLOB_FLAG_LZ4_BLOCKS
0x00000004  BLOB_FLAG_HAS_LOAD_ADDR
```

If `BLOB_FLAG_HAS_LOAD_ADDR` is set and `load_addr != 0`, `blob_load` expands
the payload to `load_addr`. Otherwise the caller-supplied fallback destination
is used.

`block_table_off` points to `block_count` entries:

```
offset  size  name
0x00    4     uncompressed_off
0x04    4     uncompressed_size
0x08    4     compressed_off
0x0c    4     compressed_size
0x10    4     compressed_crc32
```

Each uncompressed block is independently compressed as one raw LZ4 block.
`uncompressed_size` is normally 4096 bytes; only the final block may be shorter.
`compressed_off` is relative to `data_off`.

The decompression service lives in the shared service table installed by stage1
at the top of DRAM. It uses the shared heap for temporary staging, checks each
compressed block CRC with retries, expands the block to the final destination,
then frees the staging buffer.

# ROM payload directory

The canonical C definition is `src/shared_service/service_table.h`.
`scripts/build/gen_rom.py` builds the ROM header and payload directory from a
blob list, and stage1 copies it into the shared payload manifest.

The directory starts at the beginning of the 256 KiB ROM image and is limited by
`SHARED_ROM_DIRECTORY_BYTES` (`0x2000`). Its header and entry sizes are fixed by
`SHARED_ROM_DIRECTORY_HEADER_SIZE` and `SHARED_ROM_DIRECTORY_ENTRY_SIZE`, and
stage1 validates them exactly.

Directory checksum is the 32-bit word sum over header plus populated entries.
`checksum` is chosen so the sum is zero.
