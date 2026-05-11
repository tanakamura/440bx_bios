#ifndef BLOB_H
#define BLOB_H

#define BLOB_MAGIC 0x345a4c42u
#define BLOB_VERSION 2u
#define BLOB_FLAG_LZ4_BLOCKS 0x00000002u
#define BLOB_BLOCK_SIZE 4096u

#define BLOB_SERVICE_LINEAR 0x00180000u
#define BLOB_STAGE_LINEAR 0x00380000u
#define BLOB_STAGE_CAPACITY 8192u

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
                              struct blob_status* status);

#endif
