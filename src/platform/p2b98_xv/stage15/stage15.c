#include "blob.h"
#include "post_code.h"
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
extern unsigned int blob_crc32_service(const void* data, unsigned int len);

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

static void serial_write_string(const char* s) {
    while (*s != '\0') {
        serial_write_char(*s);
        ++s;
    }
}

static void serial_write_hex4(unsigned char value) {
    value &= 0x0f;
    serial_write_char(
        (char)(value < 10 ? ('0' + value) : ('a' + (value - 10))));
}

static void serial_write_hex8(unsigned char value) {
    serial_write_hex4((unsigned char)(value >> 4));
    serial_write_hex4(value);
}

static void serial_write_hex16(unsigned short value) {
    serial_write_hex8((unsigned char)(value >> 8));
    serial_write_hex8((unsigned char)value);
}

static void serial_write_hex32(unsigned int value) {
    serial_write_hex16((unsigned short)(value >> 16));
    serial_write_hex16((unsigned short)value);
}

static void die_with_post(unsigned char code) {
    outb(0x80, code);
    for (;;) {
        __asm__ volatile("hlt");
    }
}

static unsigned int dram_stack_top(unsigned int total_bytes) {
    if (total_bytes >= 0x00300000u) {
        return shared_table_base_from_total(total_bytes);
    }
    return 0x001ff000u;
}

static void install_shared_service_table(unsigned int total_bytes,
                                         unsigned int stack_top) {
    unsigned int table_linear = shared_table_base_from_total(total_bytes);
    unsigned int ptr_slot = shared_table_pointer_slot(total_bytes);
    struct shared_service_table* table =
        (struct shared_service_table*)table_linear;
    struct shared_payload_manifest* manifest;
    struct shared_boot_context* ctx;
    unsigned char* heap_base;
    unsigned int service_base = (unsigned int)__blob_service_start;
    unsigned int i;

    for (i = 0u; i < SHARED_TABLE_BYTES; ++i) {
        ((volatile unsigned char*)table_linear)[i] = 0u;
    }

    manifest = (struct shared_payload_manifest*)shared_align_up(
        table_linear + sizeof(*table), 16u);
    ctx = (struct shared_boot_context*)shared_align_up(
        (unsigned int)(manifest + 1), 16u);
    heap_base = (unsigned char*)shared_align_up((unsigned int)(ctx + 1), 16u);

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
    table->heap_base = (unsigned int)heap_base;
    table->heap_limit = ptr_slot;
    shared_heap_init(table);
    table->boot_context_ptr = (unsigned int)ctx;
    table->payload_manifest_ptr = (unsigned int)manifest;
    table->blob_load = (unsigned int)blob_load_service;
    table->heap_alloc = (unsigned int)shared_heap_alloc_service;
    table->heap_free = (unsigned int)shared_heap_free_service;
    table->heap_realloc = (unsigned int)shared_heap_realloc_service;
    table->crc32 = (unsigned int)blob_crc32_service;

    if (shared_payload_manifest_from_rom_directory(manifest,
                                                   SHARED_ROM_HIGH_BASE) != 0) {
        manifest->magic = SHARED_PAYLOAD_MAGIC;
        manifest->version = SHARED_PAYLOAD_VERSION;
        manifest->entry_count = 0u;
        manifest->reserved = 0u;
    }

    ctx->magic = SHARED_BOOT_CONTEXT_MAGIC;
    ctx->version = SHARED_BOOT_CONTEXT_VERSION;
    ctx->size = sizeof(*ctx);
    ctx->total_dram_bytes = total_bytes;
    ctx->flags = SHARED_BOOT_FLAG_PLATFORM_P2B98_XV;
    ctx->platform_id = SHARED_BOOT_FLAG_PLATFORM_P2B98_XV;
    {
        struct shared_payload_entry* dsdt_payload =
            shared_payload_find(table, SHARED_PAYLOAD_ID_DSDT);
        if (dsdt_payload != 0) {
            ctx->acpi_input_ptr = dsdt_payload->blob_ptr;
            ctx->acpi_input_size = dsdt_payload->blob_size;
        }
    }

    *(volatile unsigned int*)ptr_slot = (unsigned int)table;
}

static void enter_stage2(unsigned int total_bytes) {
    typedef void (*stage2_entry_fn)(unsigned int);
    struct blob_status status;
    struct shared_service_table* service =
        shared_service_from_total(total_bytes);
    blob_load_fn load = 0;
    unsigned int stage2_load = STAGE2_LOAD_LINEAR;
    int rc;

    if (service != 0) {
        load = (blob_load_fn)service->blob_load;
    }

    if (load == 0) {
        serial_write_string("blobsvc missing\r\n");
        die_with_post(0xef);
    }

    rc = load(SHARED_PAYLOAD_ID_STAGE2, (void*)STAGE2_LOAD_LINEAR,
              STAGE2_LOAD_CAPACITY, &stage2_load, &status, total_bytes);
    if (rc != 0) {
        serial_write_string("\r\nstage2 load failed rc=");
        serial_write_hex8((unsigned char)rc);
        serial_write_string(" block=");
        serial_write_hex32(status.block);
        serial_write_string(" exp=");
        serial_write_hex32(status.expected);
        serial_write_string(" got=");
        serial_write_hex32(status.got);
        serial_write_string("\r\n");
        die_with_post(0xef);
    }

    serial_write_string("\r\nstage2 copied\r\n");
    ((stage2_entry_fn)stage2_load)(total_bytes);
    die_with_post(0xef);
}

void stage15_main(unsigned int total_bytes) {
    unsigned int stack_top = dram_stack_top(total_bytes);

    serial_write_string("stage1.5 @ ");
    serial_write_hex32(STAGE15_LOAD_LINEAR);
    serial_write_string("\r\n");
    install_shared_service_table(total_bytes, stack_top);
    serial_write_string("svctab ok\r\n");
    enter_stage2(total_bytes);
}
