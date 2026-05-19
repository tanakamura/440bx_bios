#include "blob.h"
#include "shared_service/service_table.h"

#define ROM_HIGH_DELTA 0xfff00000u

extern unsigned char __bios_blob_start[] __attribute__((weak));
extern unsigned char __bios_blob_end[] __attribute__((weak));
extern unsigned char __stage2_blob_start[] __attribute__((weak));
extern unsigned char __stage2_blob_end[] __attribute__((weak));
extern unsigned char __test_elf_blob_start[] __attribute__((weak));
extern unsigned char __test_elf_blob_end[] __attribute__((weak));
extern unsigned char __blob_service_start[];
extern unsigned char __blob_service_end[];
extern int blob_expand_service(const void* blob, void* stage, void* dst,
                               unsigned int dst_capacity,
                               struct blob_status* status,
                               unsigned int total_bytes);

static const unsigned char* rom_high_ptr(const unsigned char* ptr) {
    return (const unsigned char*)((unsigned int)ptr + ROM_HIGH_DELTA);
}

static void payload_add(struct shared_payload_manifest* manifest,
                        unsigned int id, unsigned int type, unsigned int flags,
                        const unsigned char* start,
                        const unsigned char* end) {
    struct shared_payload_entry* entry;

    if (start == end || manifest->entry_count >= SHARED_PAYLOAD_MAX) {
        return;
    }

    entry = &manifest->entries[manifest->entry_count++];
    entry->id = id;
    entry->type = type;
    entry->flags = flags;
    entry->blob_ptr = (unsigned int)start;
    entry->blob_size = (unsigned int)(end - start);
    entry->slot_size = entry->blob_size;
}

void qemu_install_shared_service_table(unsigned int total_bytes,
                                       unsigned int stack_top,
                                       unsigned int service_base,
                                       unsigned int blob_stage) {
    unsigned int table_linear = shared_table_base_from_total(total_bytes);
    unsigned int ptr_slot = shared_table_pointer_slot(total_bytes);
    struct shared_service_table* table =
        (struct shared_service_table*)table_linear;
    struct shared_payload_manifest* manifest;
    struct shared_boot_context* ctx;
    unsigned int i;

    for (i = 0; i < SHARED_TABLE_BYTES; ++i) {
        ((volatile unsigned char*)table_linear)[i] = 0u;
    }

    manifest = (struct shared_payload_manifest*)shared_align_up(
        table_linear + sizeof(*table), 16u);
    ctx = (struct shared_boot_context*)shared_align_up(
        (unsigned int)(manifest + 1), 16u);

    table->magic = SHARED_SERVICE_MAGIC;
    table->version = SHARED_SERVICE_VERSION;
    table->size = sizeof(*table);
    table->total_dram_bytes = total_bytes;
    table->service_base = service_base;
    table->service_size =
        (unsigned int)(__blob_service_end - __blob_service_start);
    table->table_linear = table_linear;
    table->table_size = SHARED_TABLE_BYTES;
    table->stack_top = stack_top;
    table->heap_base = shared_align_up((unsigned int)(ctx + 1), 16u);
    table->heap_free_list = 0u;
    table->heap_limit = ptr_slot;
    table->boot_context_ptr = (unsigned int)ctx;
    table->payload_manifest_ptr = (unsigned int)manifest;
    table->blob_expand =
        service_base +
        ((unsigned int)blob_expand_service - (unsigned int)__blob_service_start);
    table->blob_stage = blob_stage;
    table->blob_stage_size = BLOB_STAGE_CAPACITY;

    if (shared_payload_manifest_from_rom_directory(manifest, 0xfffc0000u) !=
        0) {
        manifest->magic = SHARED_PAYLOAD_MAGIC;
        manifest->version = SHARED_PAYLOAD_VERSION;
        manifest->entry_count = 0u;
        payload_add(manifest, SHARED_PAYLOAD_ID_STAGE2,
                    SHARED_PAYLOAD_TYPE_BLZ4, 0u,
                    rom_high_ptr(__stage2_blob_start),
                    rom_high_ptr(__stage2_blob_end));
        payload_add(manifest, SHARED_PAYLOAD_ID_STAGE3,
                    SHARED_PAYLOAD_TYPE_BLZ4, 0u,
                    rom_high_ptr(__bios_blob_start),
                    rom_high_ptr(__bios_blob_end));
        payload_add(manifest, SHARED_PAYLOAD_ID_TEST_ELF,
                    SHARED_PAYLOAD_TYPE_BLZ4, 0u,
                    rom_high_ptr(__test_elf_blob_start),
                    rom_high_ptr(__test_elf_blob_end));
    }

    ctx->magic = SHARED_BOOT_CONTEXT_MAGIC;
    ctx->version = SHARED_BOOT_CONTEXT_VERSION;
    ctx->size = sizeof(*ctx);
    ctx->total_dram_bytes = total_bytes;
    ctx->flags = SHARED_BOOT_FLAG_PLATFORM_QEMU;
    ctx->platform_id = SHARED_BOOT_FLAG_PLATFORM_QEMU;

    *(volatile unsigned int*)ptr_slot = (unsigned int)table;
}
