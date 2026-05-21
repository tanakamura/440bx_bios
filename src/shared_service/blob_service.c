#include "blob.h"
#include "shared_service/service_table.h"

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
static inline void outl(unsigned short port, unsigned int value) {
    __asm__ volatile("outl %0, %1" : : "a"(value), "Nd"(port));
}
static inline unsigned int inl(unsigned short port) {
    unsigned int value;
    __asm__ volatile("inl %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static void serial_write_char(char c) {
    while ((inb(0x03f8 + 5) & 0x20) == 0) {
    }
    outb(0x03f8, (unsigned char)c);
}

static BLOBSVC void blob_set_maintenance_request(unsigned int total_bytes) {
    unsigned int ptr_slot;
    volatile unsigned int* slot;
    struct shared_service_table* table;
    struct shared_boot_context* ctx;

    if (total_bytes < 0x1000u) {
        return;
    }

    ptr_slot = (total_bytes & ~3u) - 4u;
    slot = (volatile unsigned int*)ptr_slot;
    table = (struct shared_service_table*)*slot;
    if (table == 0 || ((unsigned int)table & 0x0fu) != 0u ||
        (unsigned int)table >= ptr_slot || (unsigned int)table < 0x1000u ||
        table->magic != SHARED_SERVICE_MAGIC ||
        table->version != SHARED_SERVICE_VERSION ||
        table->size < sizeof(*table) || table->boot_context_ptr == 0u) {
        return;
    }

    ctx = (struct shared_boot_context*)table->boot_context_ptr;
    if (ctx->magic != SHARED_BOOT_CONTEXT_MAGIC ||
        ctx->version != SHARED_BOOT_CONTEXT_VERSION ||
        ctx->size < sizeof(*ctx)) {
        return;
    }

    ctx->flags |= SHARED_BOOT_FLAG_MAINTENANCE_REQUESTED;
}

static BLOBSVC void blob_check_maintenance_key(unsigned int total_bytes) {
    unsigned char ch;

    if ((inb(0x03f8 + 5) & 0x01u) == 0) {
        return;
    }
    ch = inb(0x03f8);
    if (ch == 'm' || ch == 'M') {
        blob_set_maintenance_request(total_bytes);
    }
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

static BLOBSVC_INLINE unsigned int heap_payload_size(
    const struct shared_heap_block* block) {
    if (block->size <= sizeof(*block)) {
        return 0u;
    }
    return block->size - sizeof(*block);
}

static BLOBSVC_INLINE void heap_copy(unsigned char* dst,
                                     const unsigned char* src,
                                     unsigned int size) {
    unsigned int i;
    for (i = 0; i < size; ++i) {
        dst[i] = src[i];
    }
}

static BLOBSVC void heap_coalesce_next(struct shared_heap_block* block) {
    struct shared_heap_block* next;
    unsigned int block_addr = (unsigned int)block;

    if (block->next == 0u || block_addr + block->size != block->next) {
        return;
    }
    next = (struct shared_heap_block*)block->next;
    block->size += next->size;
    block->next = next->next;
}

BLOBSVC_ENTRY void* shared_heap_alloc_service(unsigned int total_bytes,
                                              unsigned int size) {
    struct shared_service_table* table = shared_service_from_total(total_bytes);
    unsigned int needed;
    unsigned int prev = 0u;
    unsigned int cur;

    if (table == 0 || size == 0u) {
        return 0;
    }
    needed = shared_align_up(size + sizeof(struct shared_heap_block),
                             SHARED_HEAP_ALIGN);
    if (needed < SHARED_HEAP_MIN_BLOCK) {
        needed = SHARED_HEAP_MIN_BLOCK;
    }

    cur = table->heap_free_list;
    while (cur != 0u) {
        struct shared_heap_block* block = (struct shared_heap_block*)cur;
        unsigned int next = block->next;
        if (block->size >= needed) {
            unsigned int remain = block->size - needed;
            if (remain >= SHARED_HEAP_MIN_BLOCK) {
                struct shared_heap_block* split =
                    (struct shared_heap_block*)(cur + needed);
                split->size = remain;
                split->next = next;
                split->reserved0 = 0u;
                split->reserved1 = 0u;
                if (prev != 0u) {
                    ((struct shared_heap_block*)prev)->next =
                        (unsigned int)split;
                } else {
                    table->heap_free_list = (unsigned int)split;
                }
                block->size = needed;
            } else {
                if (prev != 0u) {
                    ((struct shared_heap_block*)prev)->next = next;
                } else {
                    table->heap_free_list = next;
                }
            }
            block->next = 0u;
            block->reserved0 = 0u;
            block->reserved1 = 0u;
            return (void*)(cur + sizeof(*block));
        }
        prev = cur;
        cur = next;
    }
    return 0;
}

BLOBSVC_ENTRY void shared_heap_free_service(unsigned int total_bytes,
                                            void* ptr) {
    struct shared_service_table* table = shared_service_from_total(total_bytes);
    struct shared_heap_block* block;
    unsigned int addr;
    unsigned int prev = 0u;
    unsigned int cur;

    if (table == 0 || ptr == 0) {
        return;
    }
    addr = (unsigned int)ptr - sizeof(struct shared_heap_block);
    if ((addr & (SHARED_HEAP_ALIGN - 1u)) != 0u ||
        addr < table->heap_base ||
        addr + sizeof(struct shared_heap_block) > table->heap_limit) {
        return;
    }
    block = (struct shared_heap_block*)addr;
    if (block->size < SHARED_HEAP_MIN_BLOCK ||
        addr + block->size > table->heap_limit) {
        return;
    }

    cur = table->heap_free_list;
    while (cur != 0u && cur < addr) {
        prev = cur;
        cur = ((struct shared_heap_block*)cur)->next;
    }
    block->next = cur;
    if (prev != 0u) {
        ((struct shared_heap_block*)prev)->next = addr;
        heap_coalesce_next((struct shared_heap_block*)prev);
    } else {
        table->heap_free_list = addr;
    }
    heap_coalesce_next(block);
}

BLOBSVC_ENTRY void* shared_heap_realloc_service(unsigned int total_bytes,
                                                void* ptr,
                                                unsigned int size) {
    struct shared_heap_block* block;
    void* new_ptr;
    unsigned int old_size;
    unsigned int copy_size;

    if (ptr == 0) {
        return shared_heap_alloc_service(total_bytes, size);
    }
    if (size == 0u) {
        shared_heap_free_service(total_bytes, ptr);
        return 0;
    }

    block = (struct shared_heap_block*)(
        (unsigned int)ptr - sizeof(struct shared_heap_block));
    old_size = heap_payload_size(block);
    if (old_size >= size) {
        return ptr;
    }

    new_ptr = shared_heap_alloc_service(total_bytes, size);
    if (new_ptr == 0) {
        return 0;
    }
    copy_size = old_size < size ? old_size : size;
    heap_copy((unsigned char*)new_ptr, (const unsigned char*)ptr, copy_size);
    shared_heap_free_service(total_bytes, ptr);
    return new_ptr;
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
                                      struct blob_status* status,
                                      unsigned int total_bytes) {
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
    if ((hdr->flags & BLOB_FLAG_LZ4_BLOCKS) == 0u ||
        (hdr->flags & ~BLOB_FLAG_KNOWN) != 0u) {
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
        blob_check_maintenance_key(total_bytes);
        for (retry = 0; retry < 64u; ++retry) {
            unsigned int j;
            for (j = 0; j < block->compressed_size; ++j) {
                if ((j & 0xffu) == 0u) {
                    blob_check_maintenance_key(total_bytes);
                }
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
        blob_check_maintenance_key(total_bytes);
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

    if (hdr->uncompressed_size <= BLOB_OUTPUT_CRC_LIMIT) {
        unsigned int output_crc = blob_crc32(dst, hdr->uncompressed_size);
        if (output_crc != hdr->uncompressed_crc32) {
            blob_status_set(status, BLOB_ERR_OUTPUT_CRC, hdr->block_count,
                            hdr->uncompressed_crc32, output_crc,
                            hdr->uncompressed_size);
            return BLOB_ERR_OUTPUT_CRC;
        }
    }

    blob_status_set(status, BLOB_STATUS_OK, 0, 0, 0, hdr->uncompressed_size);
    return 0;
}

BLOBSVC_ENTRY int blob_load_service(unsigned int payload_id, void* fallback_dst,
                                    unsigned int dst_capacity,
                                    unsigned int* load_addr_out,
                                    struct blob_status* status,
                                    unsigned int total_bytes) {
    struct shared_service_table* service = shared_service_from_total(total_bytes);
    struct shared_payload_entry* payload;
    const void* blob;
    void* stage;
    unsigned int load_addr;
    int rc;

    if (service == 0) {
        blob_status_set(status, BLOB_ERR_SIZE, 0, 0, 0, 0);
        return BLOB_ERR_SIZE;
    }

    payload = shared_payload_find(service, payload_id);
    if (payload == 0 || payload->type != SHARED_PAYLOAD_TYPE_BLZ4 ||
        payload->blob_ptr == 0u) {
        blob_status_set(status, BLOB_ERR_MAGIC, 0, payload_id, 0, 0);
        return BLOB_ERR_MAGIC;
    }

    blob = (const void*)payload->blob_ptr;
    load_addr = blob_load_addr_or(blob, (unsigned int)fallback_dst);
    if (load_addr_out != 0) {
        *load_addr_out = load_addr;
    }

    stage = shared_heap_alloc_service(total_bytes, BLOB_STAGE_CAPACITY);
    if (stage == 0) {
        blob_status_set(status, BLOB_ERR_SIZE, 0, 0, 0, 0);
        return BLOB_ERR_SIZE;
    }

    rc = blob_expand_service(blob, stage, (void*)load_addr, dst_capacity,
                             status, total_bytes);
    shared_heap_free_service(total_bytes, stage);
    return rc;
}

#define IA32_MTRR_FIX4K_E0000 0x26cu
#define IA32_MTRR_FIX4K_E8000 0x26du
#define IA32_MTRR_FIX4K_F0000 0x26eu
#define IA32_MTRR_FIX4K_F8000 0x26fu
#define IA32_MTRR_DEF_TYPE 0x2ffu
#define MTRR_DEF_TYPE_E 0x00000800u

static BLOBSVC_INLINE unsigned int blob_pci_addr(unsigned char bus,
                                                 unsigned char dev,
                                                 unsigned char fn,
                                                 unsigned char reg) {
    return 0x80000000u | ((unsigned int)bus << 16) |
           ((unsigned int)dev << 11) | ((unsigned int)fn << 8) |
           (reg & 0xfcu);
}

static BLOBSVC void blob_pci_write8(unsigned char bus, unsigned char dev,
                                    unsigned char fn, unsigned char reg,
                                    unsigned char value) {
    outl(0x0cf8u, blob_pci_addr(bus, dev, fn, reg));
    outb((unsigned short)(0x0cfcu + (reg & 3u)), value);
}

static BLOBSVC_INLINE void blob_wrmsr64(unsigned int msr, unsigned int lo,
                                        unsigned int hi) {
    __asm__ volatile("wrmsr" : : "c"(msr), "a"(lo), "d"(hi));
}

static BLOBSVC_INLINE unsigned long long blob_rdmsr64(unsigned int msr) {
    unsigned int lo;
    unsigned int hi;
    __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((unsigned long long)hi << 32) | lo;
}

static BLOBSVC void blob_enable_ef_shadow(void) {
    unsigned long long def_type = blob_rdmsr64(IA32_MTRR_DEF_TYPE);
    unsigned int def_lo = (unsigned int)def_type;
    unsigned int def_hi = (unsigned int)(def_type >> 32);

    __asm__ volatile("movl %%cr0, %%eax\n\t"
                     "orl $0x40000000, %%eax\n\t"
                     "andl $0xdfffffff, %%eax\n\t"
                     "movl %%eax, %%cr0\n\t"
                     "wbinvd"
                     :
                     :
                     : "eax", "memory");

    blob_pci_write8(0, 0, 0, 0x5eu, 0x33u);
    blob_pci_write8(0, 0, 0, 0x5fu, 0x33u);
    blob_pci_write8(0, 0, 0, 0x59u, 0x30u);

    blob_wrmsr64(IA32_MTRR_DEF_TYPE, def_lo & ~MTRR_DEF_TYPE_E, def_hi);
    blob_wrmsr64(IA32_MTRR_FIX4K_E0000, 0x06060606u, 0x06060606u);
    blob_wrmsr64(IA32_MTRR_FIX4K_E8000, 0x06060606u, 0x06060606u);
    blob_wrmsr64(IA32_MTRR_FIX4K_F0000, 0x06060606u, 0x06060606u);
    blob_wrmsr64(IA32_MTRR_FIX4K_F8000, 0x06060606u, 0x06060606u);
    blob_wrmsr64(IA32_MTRR_DEF_TYPE, def_lo, def_hi);

    __asm__ volatile("wbinvd\n\t"
                     "movl %%cr0, %%eax\n\t"
                     "andl $0x9fffffff, %%eax\n\t"
                     "movl %%eax, %%cr0\n\t"
                     "xorl %%eax, %%eax\n\t"
                     "cpuid"
                     :
                     :
                     : "eax", "ebx", "ecx", "edx", "memory");
}

BLOBSVC void blob_shadow_load_and_enter(const void* blob, void* stage,
                                        void* dst,
                                        unsigned int dst_capacity,
                                        struct blob_status* status,
                                        unsigned int total_bytes,
                                        unsigned int fdos_blob_linear,
                                        unsigned int bios_entry) {
    typedef void (*bios_entry_fn)(unsigned int, unsigned int);
    struct shared_service_table* service;
    struct shared_payload_entry* payload;
    volatile unsigned int* p;
    volatile unsigned int* end;
    blob_load_fn load = 0;
    int rc;

    blob_enable_ef_shadow();

    service = shared_service_from_total(total_bytes);
    payload = shared_payload_find(service, SHARED_PAYLOAD_ID_STAGE2);
    if (payload != 0) {
        blob = (const void*)payload->blob_ptr;
    }
    bios_entry = blob_load_addr_or(blob, bios_entry);
    dst = (unsigned char*)bios_entry;

    p = (volatile unsigned int*)dst;
    end = (volatile unsigned int*)((unsigned int)dst + dst_capacity);
    while (p < end) {
        *p++ = 0u;
    }

    if (service != 0 && service->blob_load != 0u) {
        load = (blob_load_fn)service->blob_load;
    }
    if (load != 0) {
        rc = load(SHARED_PAYLOAD_ID_STAGE2, dst, dst_capacity, &bios_entry,
                  status, total_bytes);
    } else {
        rc = blob_expand_service(blob, stage, dst, dst_capacity, status,
                                 total_bytes);
    }
    if (rc != 0) {
        outb(0x0080u, 0xefu);
        for (;;) {
            __asm__ volatile("hlt");
        }
    }

    __asm__ volatile("xorl %%eax, %%eax\n\tcpuid"
                     :
                     :
                     : "eax", "ebx", "ecx", "edx", "memory");
    ((bios_entry_fn)bios_entry)(total_bytes, fdos_blob_linear);
    for (;;) {
        __asm__ volatile("hlt");
    }
}
