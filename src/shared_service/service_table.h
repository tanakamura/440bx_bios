#ifndef SERVICE_TABLE_H
#define SERVICE_TABLE_H

#define SHARED_SERVICE_MAGIC 0x53565342u
#define SHARED_SERVICE_VERSION 1u
#define SHARED_PAYLOAD_MAGIC 0x504c4242u
#define SHARED_PAYLOAD_VERSION 1u
#define SHARED_BOOT_CONTEXT_MAGIC 0x42544358u
#define SHARED_BOOT_CONTEXT_VERSION 1u

#define SHARED_PAYLOAD_MAX 16u
#define SHARED_TABLE_BYTES 0x4000u
#define SHARED_HEAP_ALIGN 16u
#define SHARED_HEAP_MIN_BLOCK 32u

#define SHARED_PAYLOAD_ID_STAGE2 1u
#define SHARED_PAYLOAD_ID_STAGE3 2u
#define SHARED_PAYLOAD_ID_LEGACY_APP 3u
#define SHARED_PAYLOAD_ID_LINUX_LOADER_APP 4u
#define SHARED_PAYLOAD_ID_VGABIOS 5u
#define SHARED_PAYLOAD_ID_DSDT 6u
#define SHARED_PAYLOAD_ID_TEST_ELF 7u
#define SHARED_PAYLOAD_ID_SELFTEST_APP 8u
#define SHARED_PAYLOAD_ID_TEST_FLOPPY 9u

#define SHARED_PAYLOAD_TYPE_BLZ4 1u
#define SHARED_PAYLOAD_TYPE_APP 2u
#define SHARED_PAYLOAD_TYPE_RAW 3u

#define SHARED_ROM_DIRECTORY_MAGIC 0x304d5242u
#define SHARED_ROM_DIRECTORY_VERSION 1u
#define SHARED_ROM_DIRECTORY_ENTRY_MAX SHARED_PAYLOAD_MAX
#define SHARED_ROM_SIZE (256u * 1024u)

#define SHARED_BOOT_FLAG_SHADOW_READY 0x00000001u
#define SHARED_BOOT_FLAG_MAINTENANCE_REQUESTED 0x00000002u
#define SHARED_BOOT_FLAG_PLATFORM_QEMU 0x00000100u
#define SHARED_BOOT_FLAG_PLATFORM_P2B98_XV 0x00000200u

#define SHARED_BOOT_ACPI_FLAG_ENABLE_SCI 0x00000001u

struct shared_payload_entry {
    unsigned int id;
    unsigned int type;
    unsigned int flags;
    unsigned int blob_ptr;
    unsigned int blob_size;
    unsigned int slot_size;
};

struct shared_payload_manifest {
    unsigned int magic;
    unsigned int version;
    unsigned int entry_count;
    unsigned int reserved;
    struct shared_payload_entry entries[SHARED_PAYLOAD_MAX];
};

struct shared_rom_payload_directory {
    unsigned int magic;
    unsigned int version;
    unsigned int header_size;
    unsigned int entry_size;
    unsigned int entry_count;
    unsigned int payload_area_start;
    unsigned int payload_area_end;
    unsigned int stage1_start;
};

struct shared_rom_payload_directory_entry {
    unsigned int id;
    unsigned int type;
    unsigned int flags;
    unsigned int rom_offset;
    unsigned int blob_size;
    unsigned int slot_size;
    unsigned int load_addr;
    unsigned int reserved;
};

struct shared_boot_context {
    unsigned int magic;
    unsigned int version;
    unsigned int size;
    unsigned int total_dram_bytes;
    unsigned int flags;
    unsigned int platform_id;
    unsigned int acpi_input_ptr;
    unsigned int acpi_input_size;
    unsigned int rsdp_linear;
    unsigned int pci_io_base;
    unsigned int pci_io_limit;
    unsigned int pci_mem_base;
    unsigned int pci_mem_limit;
    unsigned int pci_prefetch_mem_base;
    unsigned int pci_prefetch_mem_limit;
    unsigned int acpi_pm1_evt;
    unsigned int acpi_pm1_cnt;
    unsigned int acpi_gpe0;
    unsigned int acpi_gpe0_len;
    unsigned int acpi_flags;
};

struct shared_service_table {
    unsigned int magic;
    unsigned int version;
    unsigned int size;
    unsigned int checksum;
    unsigned int total_dram_bytes;
    unsigned int service_base;
    unsigned int service_size;
    unsigned int table_linear;
    unsigned int table_size;
    unsigned int stack_top;
    unsigned int heap_base;
    unsigned int heap_free_list;
    unsigned int heap_limit;
    unsigned int boot_context_ptr;
    unsigned int payload_manifest_ptr;
    unsigned int blob_load;
    unsigned int heap_alloc;
    unsigned int heap_free;
    unsigned int heap_realloc;
    unsigned int crc32;
    unsigned int memcpy_fn;
    unsigned int memset_fn;
    unsigned int serial_write;
    unsigned int post_code;
    unsigned int panic;
};

struct shared_heap_block {
    unsigned int size;
    unsigned int next;
    unsigned int reserved0;
    unsigned int reserved1;
};

typedef void* (*shared_heap_alloc_fn)(unsigned int total_bytes,
                                      unsigned int size);
typedef void (*shared_heap_free_fn)(unsigned int total_bytes, void* ptr);
typedef void* (*shared_heap_realloc_fn)(unsigned int total_bytes, void* ptr,
                                        unsigned int size);

static inline unsigned int shared_align_up(unsigned int value,
                                           unsigned int align) {
    return (value + align - 1u) & ~(align - 1u);
}

static inline unsigned int shared_align_down(unsigned int value,
                                             unsigned int align) {
    return value & ~(align - 1u);
}

static inline void shared_heap_init(struct shared_service_table* table) {
    unsigned int base;
    unsigned int limit;
    struct shared_heap_block* block;

    if (table == 0) {
        return;
    }
    base = shared_align_up(table->heap_base, SHARED_HEAP_ALIGN);
    limit = shared_align_down(table->heap_limit, SHARED_HEAP_ALIGN);
    table->heap_base = base;
    table->heap_limit = limit;
    table->heap_free_list = 0u;
    if (limit < base + sizeof(*block) ||
        limit - base < SHARED_HEAP_MIN_BLOCK) {
        return;
    }
    block = (struct shared_heap_block*)base;
    block->size = limit - base;
    block->next = 0u;
    block->reserved0 = 0u;
    block->reserved1 = 0u;
    table->heap_free_list = base;
}

static inline unsigned int shared_table_pointer_slot(unsigned int total_bytes) {
    return (total_bytes & ~3u) - 4u;
}

static inline unsigned int shared_table_base_from_total(
    unsigned int total_bytes) {
    return (total_bytes & ~0xfffu) - SHARED_TABLE_BYTES;
}

static inline unsigned int shared_service_base_from_table(
    unsigned int table_base, unsigned int service_size) {
    return (table_base - shared_align_up(service_size, 16u)) & ~0x0fu;
}

static inline struct shared_service_table* shared_service_from_total(
    unsigned int total_bytes) {
    volatile unsigned int* slot =
        (volatile unsigned int*)shared_table_pointer_slot(total_bytes);
    struct shared_service_table* table = (struct shared_service_table*)*slot;

    if (table == 0 || ((unsigned int)table & 0x0fu) != 0u ||
        (unsigned int)table >= shared_table_pointer_slot(total_bytes) ||
        (unsigned int)table < 0x1000u ||
        table->magic != SHARED_SERVICE_MAGIC ||
        table->version != SHARED_SERVICE_VERSION ||
        table->size < sizeof(*table)) {
        return 0;
    }
    return table;
}

static inline struct shared_payload_entry* shared_payload_find(
    const struct shared_service_table* table, unsigned int id) {
    struct shared_payload_manifest* manifest;
    unsigned int i;

    if (table == 0 || table->payload_manifest_ptr == 0) {
        return 0;
    }
    manifest = (struct shared_payload_manifest*)table->payload_manifest_ptr;
    if (manifest->magic != SHARED_PAYLOAD_MAGIC ||
        manifest->version != SHARED_PAYLOAD_VERSION ||
        manifest->entry_count > SHARED_PAYLOAD_MAX) {
        return 0;
    }
    for (i = 0; i < manifest->entry_count; ++i) {
        if (manifest->entries[i].id == id) {
            return &manifest->entries[i];
        }
    }
    return 0;
}

static inline int shared_payload_manifest_from_rom_directory(
    struct shared_payload_manifest* manifest, unsigned int rom_high_base) {
    const struct shared_rom_payload_directory* dir =
        (const struct shared_rom_payload_directory*)rom_high_base;
    const unsigned char* rom = (const unsigned char*)rom_high_base;
    unsigned int i;

    if (manifest == 0 || dir->magic != SHARED_ROM_DIRECTORY_MAGIC ||
        dir->version != SHARED_ROM_DIRECTORY_VERSION ||
        dir->header_size < sizeof(*dir) ||
        dir->entry_size < sizeof(struct shared_rom_payload_directory_entry) ||
        dir->entry_count > SHARED_ROM_DIRECTORY_ENTRY_MAX ||
        dir->payload_area_start < dir->header_size ||
        dir->payload_area_end > SHARED_ROM_SIZE ||
        dir->payload_area_start > dir->payload_area_end ||
        dir->stage1_start > SHARED_ROM_SIZE ||
        dir->payload_area_end > dir->stage1_start) {
        return -1;
    }

    manifest->magic = SHARED_PAYLOAD_MAGIC;
    manifest->version = SHARED_PAYLOAD_VERSION;
    manifest->entry_count = 0u;
    manifest->reserved = 0u;

    for (i = 0; i < dir->entry_count; ++i) {
        const struct shared_rom_payload_directory_entry* src =
            (const struct shared_rom_payload_directory_entry*)(
                rom + dir->header_size + i * dir->entry_size);
        struct shared_payload_entry* dst;

        if (src->id == 0u || src->blob_size == 0u ||
            src->rom_offset < dir->payload_area_start ||
            src->rom_offset > dir->payload_area_end ||
            src->blob_size > dir->payload_area_end - src->rom_offset ||
            src->slot_size < src->blob_size ||
            manifest->entry_count >= SHARED_PAYLOAD_MAX) {
            return -1;
        }

        dst = &manifest->entries[manifest->entry_count++];
        dst->id = src->id;
        dst->type = src->type;
        dst->flags = src->flags;
        dst->blob_ptr = rom_high_base + src->rom_offset;
        dst->blob_size = src->blob_size;
        dst->slot_size = src->slot_size;
    }
    return 0;
}

static inline struct shared_boot_context* shared_boot_context(
    const struct shared_service_table* table) {
    struct shared_boot_context* ctx;

    if (table == 0 || table->boot_context_ptr == 0) {
        return 0;
    }
    ctx = (struct shared_boot_context*)table->boot_context_ptr;
    if (ctx->magic != SHARED_BOOT_CONTEXT_MAGIC ||
        ctx->version != SHARED_BOOT_CONTEXT_VERSION ||
        ctx->size < sizeof(*ctx)) {
        return 0;
    }
    return ctx;
}

#endif
