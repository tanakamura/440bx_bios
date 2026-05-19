#ifndef SERVICE_TABLE_H
#define SERVICE_TABLE_H

#define SHARED_SERVICE_MAGIC 0x53565342u
#define SHARED_SERVICE_VERSION 1u
#define SHARED_PAYLOAD_MAGIC 0x504c4242u
#define SHARED_PAYLOAD_VERSION 1u
#define SHARED_BOOT_CONTEXT_MAGIC 0x42544358u
#define SHARED_BOOT_CONTEXT_VERSION 1u

#define SHARED_PAYLOAD_MAX 16u
#define SHARED_TABLE_BYTES 4096u

#define SHARED_PAYLOAD_ID_STAGE2 1u
#define SHARED_PAYLOAD_ID_STAGE3 2u
#define SHARED_PAYLOAD_ID_LEGACY_APP 3u
#define SHARED_PAYLOAD_ID_LINUX_LOADER_APP 4u
#define SHARED_PAYLOAD_ID_VGABIOS 5u
#define SHARED_PAYLOAD_ID_DSDT 6u
#define SHARED_PAYLOAD_ID_TEST_ELF 7u

#define SHARED_PAYLOAD_TYPE_BLZ4 1u
#define SHARED_PAYLOAD_TYPE_APP 2u

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
    unsigned int blob_expand;
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

static inline unsigned int shared_align_up(unsigned int value,
                                           unsigned int align) {
    return (value + align - 1u) & ~(align - 1u);
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
