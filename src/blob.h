#ifndef BLOB_H
#define BLOB_H

#define BLOB_MAGIC 0x345a4c42u
#define BLOB_VERSION 3u
#define BLOB_FLAG_LZ4_BLOCKS 0x00000002u
#define BLOB_FLAG_HAS_LOAD_ADDR 0x00000004u
#define BLOB_FLAG_KNOWN (BLOB_FLAG_LZ4_BLOCKS | BLOB_FLAG_HAS_LOAD_ADDR)
#define BLOB_BLOCK_SIZE 4096u
#define BLOB_OUTPUT_CRC_LIMIT (512u * 1024u)

#define BLOB_STAGE_CAPACITY 8192u

#define STAGE2_LOAD_LINEAR 0x00080000u
#define STAGE2_LOAD_CAPACITY 0x00010000u
#define STAGE2_ENTRY 0x00080000u

#define BIOS_LOAD_LINEAR 0x00200000u
#define BIOS_LOAD_CAPACITY 0x00040000u
#define BIOS32_ENTRY 0x00200000u

#define BLOB_STATUS_OK 0
#define BLOB_ERR_MAGIC -1
#define BLOB_ERR_VERSION -2
#define BLOB_ERR_FLAGS -3
#define BLOB_ERR_SIZE -4
#define BLOB_ERR_CRC -5
#define BLOB_ERR_LZ4 -6
#define BLOB_ERR_OUTPUT_CRC -7

struct blob_header {
    unsigned int magic;
    unsigned int header_size;
    unsigned int version;
    unsigned int flags;
    unsigned int load_addr;
    unsigned int uncompressed_size;
    unsigned int compressed_size;
    unsigned int block_size;
    unsigned int block_count;
    unsigned int block_table_off;
    unsigned int data_off;
    unsigned int uncompressed_crc32;
    unsigned int compressed_crc32;
};

struct blob_block {
    unsigned int uncompressed_off;
    unsigned int uncompressed_size;
    unsigned int compressed_off;
    unsigned int compressed_size;
    unsigned int compressed_crc32;
};

struct blob_status {
    int code;
    unsigned int block;
    unsigned int expected;
    unsigned int got;
    unsigned int output_size;
};

typedef int (*blob_expand_fn)(const void* blob, void* stage, void* dst,
                              unsigned int dst_capacity,
                              struct blob_status* status,
                              unsigned int total_bytes);
typedef int (*blob_load_fn)(unsigned int payload_id, void* fallback_dst,
                            unsigned int dst_capacity,
                            unsigned int* load_addr_out,
                            struct blob_status* status,
                            unsigned int total_bytes);
typedef void (*blob_shadow_entry_fn)(const void* blob, void* stage, void* dst,
                                     unsigned int dst_capacity,
                                     struct blob_status* status,
                                     unsigned int total_bytes,
                                     unsigned int aux_blob_linear,
                                     unsigned int bios_entry);

static inline unsigned int blob_load_addr_or(const void* blob,
                                             unsigned int fallback) {
    const struct blob_header* hdr = (const struct blob_header*)blob;
    if (hdr != 0 && hdr->magic == BLOB_MAGIC && hdr->version == BLOB_VERSION &&
        (hdr->flags & BLOB_FLAG_HAS_LOAD_ADDR) != 0u &&
        hdr->load_addr != 0u) {
        return hdr->load_addr;
    }
    return fallback;
}

#endif
