#include "blob.h"
#include "shared_service/service_table.h"

extern unsigned char __blob_service_start[];
extern unsigned char __blob_service_end[];
extern int blob_load_service(unsigned int payload_id, void* fallback_dst,
                             unsigned int dst_capacity,
                             unsigned int* load_addr_out,
                             struct blob_status* status,
                             unsigned int total_bytes);
extern void* shared_heap_alloc_service(unsigned int total_bytes,
                                       unsigned int size);
extern void shared_heap_free_service(unsigned int total_bytes, void* ptr);
extern void* shared_heap_realloc_service(unsigned int total_bytes, void* ptr,
                                         unsigned int size);

void qemu_install_shared_service_table(unsigned int total_bytes,
                                       unsigned int stack_top,
                                       unsigned int service_base) {
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
    table->heap_limit = ptr_slot;
    shared_heap_init(table);
    table->boot_context_ptr = (unsigned int)ctx;
    table->payload_manifest_ptr = (unsigned int)manifest;
    table->blob_load =
        service_base +
        ((unsigned int)blob_load_service - (unsigned int)__blob_service_start);
    table->heap_alloc =
        service_base +
        ((unsigned int)shared_heap_alloc_service -
         (unsigned int)__blob_service_start);
    table->heap_free =
        service_base +
        ((unsigned int)shared_heap_free_service -
         (unsigned int)__blob_service_start);
    table->heap_realloc =
        service_base +
        ((unsigned int)shared_heap_realloc_service -
         (unsigned int)__blob_service_start);

    if (shared_payload_manifest_from_rom_directory(
            manifest, SHARED_ROM_HIGH_BASE) != 0) {
        manifest->magic = SHARED_PAYLOAD_MAGIC;
        manifest->version = SHARED_PAYLOAD_VERSION;
        manifest->entry_count = 0u;
        manifest->reserved = 0u;
    }

    ctx->magic = SHARED_BOOT_CONTEXT_MAGIC;
    ctx->version = SHARED_BOOT_CONTEXT_VERSION;
    ctx->size = sizeof(*ctx);
    ctx->total_dram_bytes = total_bytes;
    ctx->flags = SHARED_BOOT_FLAG_PLATFORM_QEMU;
    ctx->platform_id = SHARED_BOOT_FLAG_PLATFORM_QEMU;

    *(volatile unsigned int*)ptr_slot = (unsigned int)table;
}
