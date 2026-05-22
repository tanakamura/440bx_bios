#include "app_boot_abi.h"
#include "bios_acpi_runtime.h"
#include "bios_memory.h"
#include "bios_nvram.h"
#include "bios_rtc.h"
#include "bios_serial.h"
#include "selftest_exec_blob.h"
#include "shared_service/service_table.h"

#define SELFTEST_BOOT_PARAMS 0x00090000u
#define SELFTEST_RSDP_LINEAR 0x0009fc00u
#define SELFTEST_E820_TABLE_OFF 0x02d0u
#define SELFTEST_E820_MAX 128u

static inline void outb(unsigned short port, unsigned char value) {
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static void selftest_memset(void* dst, unsigned char value, unsigned int len) {
    unsigned char* p = (unsigned char*)dst;
    while (len-- != 0u) {
        *p++ = value;
    }
}

static void selftest_memcpy(void* dst, const void* src, unsigned int len) {
    unsigned char* d = (unsigned char*)dst;
    const unsigned char* s = (const unsigned char*)src;
    while (len-- != 0u) {
        *d++ = *s++;
    }
}

static void selftest_put16(unsigned char* p, unsigned short value) {
    p[0] = (unsigned char)value;
    p[1] = (unsigned char)(value >> 8);
}

static void selftest_put32(unsigned char* p, unsigned int value) {
    p[0] = (unsigned char)value;
    p[1] = (unsigned char)(value >> 8);
    p[2] = (unsigned char)(value >> 16);
    p[3] = (unsigned char)(value >> 24);
}

static void selftest_prepare_platform(const struct app_boot_context* ctx) {
    bios_rtc_prepare_for_linux(bios_nvram_enable_extended_cmos);
    bios_acpi_install_for_linux(
        ctx->platform.acpi_rsdp_linear, ctx->platform.acpi_pm1_evt,
        ctx->platform.acpi_pm1_cnt, ctx->platform.acpi_gpe0,
        ctx->platform.acpi_gpe0_len, ctx->platform.acpi_flags);
}

static void selftest_prepare_boot_params(const struct app_boot_context* ctx,
                                         unsigned int entry_phys) {
    unsigned char* bp = (unsigned char*)SELFTEST_BOOT_PARAMS;
    unsigned int total_bytes = ctx->platform.total_bytes;
    unsigned int count = bios_memory_e820_entry_count(total_bytes);
    unsigned int usable_end = bios_memory_extended_usable_end(total_bytes);
    unsigned int alt_mem_kb = 0u;
    unsigned int i;

    if (count > SELFTEST_E820_MAX) {
        count = SELFTEST_E820_MAX;
    }
    if (usable_end > 0x00100000u) {
        alt_mem_kb = (usable_end - 0x00100000u) >> 10;
        if (alt_mem_kb > 0xffffu) {
            alt_mem_kb = 0xffffu;
        }
    }

    selftest_memset(bp, 0u, 4096u);
    selftest_put16(bp + 0x01e0u, (unsigned short)alt_mem_kb);
    bp[0x01e8u] = (unsigned char)count;
    for (i = 0u; i < count; ++i) {
        struct e820_entry entry;
        if (bios_memory_e820_get_entry(total_bytes, i, &entry) != 0) {
            break;
        }
        selftest_memcpy(bp + SELFTEST_E820_TABLE_OFF + i * sizeof(entry),
                        &entry, sizeof(entry));
    }

    selftest_put16(bp + 0x01feu, 0xaa55u);
    selftest_put32(bp + 0x0202u, 0x53726448u);
    selftest_put16(bp + 0x0206u, 0x020fu);
    bp[0x0210u] = 0xffu;
    bp[0x0211u] = 0x80u;
    selftest_put16(bp + 0x0224u, 0xe000u);
    selftest_put32(bp + 0x0214u, entry_phys);
    selftest_put32(bp + 0x0228u, 0u);
    selftest_put32(bp + 0x022cu, usable_end - 1u);
    selftest_put32(bp + 0x0230u, 0x00100000u);
    bp[0x0234u] = 0u;
    selftest_put32(bp + 0x0238u, 0u);
}

static const struct selftest_exec_blob_header* selftest_blob_header(void) {
    const unsigned int* desc = (const unsigned int*)(SHARED_ROM_HIGH_BASE +
                                                     SHARED_ROM_SIZE - 8u);
    unsigned int free_first = desc[0];
    unsigned int free_end = desc[1];

    if (free_first < SHARED_ROM_HIGH_BASE ||
        free_first + SELFTEST_EXEC_BLOB_HEADER_SIZE > free_end ||
        free_end > SHARED_ROM_HIGH_BASE + SHARED_ROM_SIZE - 8u) {
        return 0;
    }
    return (const struct selftest_exec_blob_header*)free_first;
}

int app_entry(const struct app_boot_context* ctx) {
    const struct selftest_exec_blob_header* hdr = selftest_blob_header();
    const unsigned char* image;
    struct app_boot_context exec_ctx;

    if (ctx == 0) {
        return APP_BOOT_RESULT_FALLBACK;
    }
    if (hdr == 0 || hdr->magic != SELFTEST_EXEC_BLOB_MAGIC ||
        hdr->image_size == 0u || hdr->load_addr < 0x00100000u) {
        serial_write_string("No selftest executable blob\r\n");
        return APP_BOOT_RESULT_FALLBACK;
    }

    image = (const unsigned char*)(hdr + 1);
    selftest_memcpy((void*)hdr->load_addr, image, hdr->image_size);
    selftest_prepare_platform(ctx);
    selftest_prepare_boot_params(ctx, hdr->load_addr);

    exec_ctx = *ctx;
    if (exec_ctx.platform.acpi_rsdp_linear == 0u) {
        exec_ctx.platform.acpi_rsdp_linear = SELFTEST_RSDP_LINEAR;
    }
    exec_ctx.boot_params_linear = SELFTEST_BOOT_PARAMS;
    exec_ctx.work_linear = hdr->load_addr;
    exec_ctx.work_size = hdr->image_size;

    serial_write_string("Run selftest executable @ ");
    serial_write_hex32(hdr->load_addr);
    serial_write_string(" size=");
    serial_write_hex32(hdr->image_size);
    serial_write_string("\r\n");
    return ((app_entry_fn)hdr->load_addr)(&exec_ctx);
}
