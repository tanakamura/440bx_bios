#ifndef SELFTEST_EXEC_BLOB_H
#define SELFTEST_EXEC_BLOB_H

#define SELFTEST_EXEC_BLOB_MAGIC 0x30425853u
#define SELFTEST_EXEC_BLOB_HEADER_SIZE 16u

struct selftest_exec_blob_header {
    unsigned int magic;
    unsigned int load_addr;
    unsigned int image_size;
    unsigned int reserved;
};

typedef char selftest_exec_blob_header_size_check[
    sizeof(struct selftest_exec_blob_header) == SELFTEST_EXEC_BLOB_HEADER_SIZE
        ? 1
        : -1];

#endif
