#include "blob.h"

#define BLOBSVC __attribute__((section(".blobsvc"), noinline, used))
#define BLOBSVC_INLINE __attribute__((always_inline)) inline
#define BLOBSVC_ENTRY __attribute__((section(".blobsvc.entry"), noinline, used))
static inline void outb(unsigned short port, unsigned char value) {
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}
static inline unsigned char inb(unsigned short port) {
    unsigned char value;
    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static void serial_write_char(char c) {
    while ((inb(0x03f8 + 5) & 0x20) == 0) {
    }
    outb(0x03f8, (unsigned char)c);
}

static BLOBSVC void blob_status_set(struct blob_status* status, int code,
                                    unsigned int block, unsigned int expected,
                                    unsigned int got,
                                    unsigned int output_size) {
    if (status == 0) {
        return;
    }
    status->code = code;
    status->block = block;
    status->expected = expected;
    status->got = got;
    status->output_size = output_size;
}

static BLOBSVC_INLINE unsigned int blob_crc32_update(unsigned int crc,
                                                     unsigned char byte) {
    unsigned int i;
    const unsigned int poly = 0xedb88320u;
    crc ^= byte;
    for (i = 0; i < 8u; ++i) {
        unsigned int mask = 0u - (crc & 1u);
        crc = (crc >> 1) ^ (poly & mask);
    }
    return crc;
}

static BLOBSVC unsigned int blob_crc32(const unsigned char* data,
                                       unsigned int len) {
    unsigned int crc = 0xffffffffu;
    unsigned int i;
    for (i = 0; i < len; ++i) {
        crc = blob_crc32_update(crc, data[i]);
    }
    return crc ^ 0xffffffffu;
}

static BLOBSVC_INLINE unsigned char blob_read_stable_u8(
    const unsigned char* ptr) {
    volatile const unsigned char* p = (volatile const unsigned char*)ptr;
    unsigned char a = p[0];
    unsigned char b = p[0];
    unsigned char c;
    unsigned char d;
    unsigned char e;

    if (a == b) {
        return a;
    }
    c = p[0];
    if (c == a || c == b) {
        return c;
    }
    d = p[0];
    if (d == a || d == b || d == c) {
        return d;
    }
    e = p[0];
    if (e == a || e == b || e == c || e == d) {
        return e;
    }
    return e;
}

static BLOBSVC int blob_lz4_read_len(const unsigned char* src,
                                     unsigned int src_len, unsigned int* ip,
                                     unsigned int base, unsigned int* out_len) {
    unsigned int len = base;
    unsigned int value;

    if (base != 15u) {
        *out_len = len;
        return 0;
    }

    do {
        if (*ip >= src_len) {
            return BLOB_ERR_LZ4;
        }
        value = src[*ip];
        *ip += 1u;
        len += value;
    } while (value == 255u);

    *out_len = len;
    return 0;
}

static BLOBSVC int blob_lz4_decode(const unsigned char* src,
                                   unsigned int src_len, unsigned char* dst,
                                   unsigned int dst_len) {
    unsigned int ip = 0;
    unsigned int op = 0;

    while (ip < src_len) {
        unsigned int token = src[ip++];
        unsigned int lit_len;
        unsigned int match_len;
        unsigned int offset;
        unsigned int i;
        int rc;

        rc = blob_lz4_read_len(src, src_len, &ip, token >> 4, &lit_len);
        if (rc != 0) {
            return rc;
        }
        if (lit_len > src_len - ip || lit_len > dst_len - op) {
            return BLOB_ERR_LZ4;
        }
        for (i = 0; i < lit_len; ++i) {
            dst[op++] = src[ip++];
        }

        if (ip == src_len) {
            break;
        }
        if (src_len - ip < 2u) {
            return BLOB_ERR_LZ4;
        }
        offset = (unsigned int)src[ip] | ((unsigned int)src[ip + 1u] << 8);
        ip += 2u;
        if (offset == 0u || offset > op) {
            return BLOB_ERR_LZ4;
        }

        rc = blob_lz4_read_len(src, src_len, &ip, token & 0x0fu, &match_len);
        if (rc != 0) {
            return rc;
        }
        match_len += 4u;
        if (match_len > dst_len - op) {
            return BLOB_ERR_LZ4;
        }
        for (i = 0; i < match_len; ++i) {
            dst[op] = dst[op - offset];
            ++op;
        }
    }

    if (ip != src_len || op != dst_len) {
        return BLOB_ERR_LZ4;
    }
    return 0;
}

BLOBSVC_ENTRY int blob_expand_service(const void* blob_ptr, void* stage_ptr,
                                      void* dst_ptr, unsigned int dst_capacity,
                                      struct blob_status* status) {
    const unsigned char* blob = (const unsigned char*)blob_ptr;
    unsigned char* stage = (unsigned char*)stage_ptr;
    unsigned char* dst = (unsigned char*)dst_ptr;
    const struct blob_header* hdr = (const struct blob_header*)blob;
    const struct blob_block* blocks;
    const unsigned char* data;
    unsigned int i;

    blob_status_set(status, BLOB_STATUS_OK, 0, 0, 0, 0);

    if (hdr->magic != BLOB_MAGIC) {
        blob_status_set(status, BLOB_ERR_MAGIC, 0, BLOB_MAGIC, hdr->magic, 0);
        return BLOB_ERR_MAGIC;
    }
    if (hdr->version != BLOB_VERSION) {
        blob_status_set(status, BLOB_ERR_VERSION, 0, BLOB_VERSION, hdr->version,
                        0);
        return BLOB_ERR_VERSION;
    }
    if (hdr->flags != BLOB_FLAG_LZ4_BLOCKS) {
        blob_status_set(status, BLOB_ERR_FLAGS, 0, BLOB_FLAG_LZ4_BLOCKS,
                        hdr->flags, 0);
        return BLOB_ERR_FLAGS;
    }
    if (hdr->header_size < sizeof(struct blob_header) ||
        hdr->block_size != BLOB_BLOCK_SIZE ||
        hdr->uncompressed_size > dst_capacity ||
        hdr->block_count != ((hdr->uncompressed_size + BLOB_BLOCK_SIZE - 1u) /
                             BLOB_BLOCK_SIZE) ||
        hdr->block_table_off < hdr->header_size ||
        hdr->data_off <
            hdr->block_table_off + hdr->block_count * sizeof(*blocks)) {
        blob_status_set(status, BLOB_ERR_SIZE, 0, 0, 0, hdr->uncompressed_size);
        return BLOB_ERR_SIZE;
    }

    blocks = (const struct blob_block*)(blob + hdr->block_table_off);
    data = blob + hdr->data_off;

    for (i = 0; i < hdr->block_count; ++i) {
        const struct blob_block* block = blocks + i;
        const unsigned char* src;
        unsigned char* out;
        unsigned int retry;
        unsigned int got = 0;

        if (block->uncompressed_off != i * BLOB_BLOCK_SIZE ||
            block->uncompressed_size > BLOB_BLOCK_SIZE ||
            block->uncompressed_off + block->uncompressed_size >
                hdr->uncompressed_size ||
            block->compressed_size > BLOB_STAGE_CAPACITY ||
            block->compressed_off + block->compressed_size >
                hdr->compressed_size) {
            blob_status_set(status, BLOB_ERR_SIZE, i, 0, 0,
                            block->uncompressed_off);
            return BLOB_ERR_SIZE;
        }

        src = data + block->compressed_off;
        out = dst + block->uncompressed_off;
        for (retry = 0; retry < 64u; ++retry) {
            unsigned int j;
            for (j = 0; j < block->compressed_size; ++j) {
                if (retry == 0u) {
                    stage[j] = ((volatile const unsigned char*)src)[j];
                } else {
                    stage[j] = blob_read_stable_u8(src + j);
                }
            }
            got = blob_crc32(stage, block->compressed_size);
            if (got == block->compressed_crc32) {
                break;
            }
            serial_write_char('x');
        }
        if (retry == 64u) {
            blob_status_set(status, BLOB_ERR_CRC, i, block->compressed_crc32,
                            got, 0);
            return BLOB_ERR_CRC;
        }
        serial_write_char('o');

        {
            int rc = blob_lz4_decode(stage, block->compressed_size, out,
                                     block->uncompressed_size);
            if (rc != 0) {
                blob_status_set(status, BLOB_ERR_LZ4, i, 0, (unsigned int)rc,
                                block->uncompressed_off);
                return BLOB_ERR_LZ4;
            }
        }
        serial_write_char('.');
        if ((i & 0xf) == 0) {
            serial_write_char('\r');
            serial_write_char('\n');
        }
    }
    serial_write_char('!');

    {
        unsigned int compressed_off = 0;
        for (i = 0; i < hdr->block_count; ++i) {
            if (blocks[i].compressed_off != compressed_off) {
                blob_status_set(status, BLOB_ERR_SIZE, i, compressed_off,
                                blocks[i].compressed_off, 0);
                return BLOB_ERR_SIZE;
            }
            compressed_off += blocks[i].compressed_size;
        }
        if (compressed_off != hdr->compressed_size) {
            blob_status_set(status, BLOB_ERR_SIZE, hdr->block_count,
                            hdr->compressed_size, compressed_off, 0);
            return BLOB_ERR_SIZE;
        }
    }

    blob_status_set(status, BLOB_STATUS_OK, 0, 0, 0, hdr->uncompressed_size);
    return 0;
}
