# BLZ4 blob format

`BIOS.elf` and the FreeDOS floppy image are stored as the same blob format.
The canonical C definition is `src/blob.h`.

All integers are little-endian 32-bit values.

```
offset  size  name
0x00    4     magic = "BLZ4" (0x345a4c42)
0x04    4     header_size = 48
0x08    4     version = 2
0x0c    4     flags = 2 (independent raw LZ4 blocks)
0x10    4     uncompressed_size
0x14    4     compressed_size
0x18    4     block_size = 4096
0x1c    4     block_count
0x20    4     block_table_off
0x24    4     data_off
0x28    4     uncompressed_crc32
0x2c    4     compressed_crc32
```

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
`uncompressed_size` is normally 4096 bytes; only the final block may be
shorter. `compressed_off` is relative to `data_off`.

The decompression service is copied to `BLOB_SERVICE_LINEAR` (`0x00180000`) in
SDRAM. Keep it in a WB DRAM range; the service is executed while copying and
expanding ROM payloads. For each block it copies only that compressed block to
`BLOB_STAGE_LINEAR` (`0x00380000`, 8192-byte staging area), checks the block CRC
with retries, expands it to the final destination, and moves on to the next
block.
