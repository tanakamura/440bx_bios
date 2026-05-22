#include "app/selftest/s3test/selftest_stage3.h"

#include "app_boot.h"
#include "app_loader.h"
#include "bios_acpi_runtime.h"
#include "bios_memory.h"
#include "bios_nvram.h"
#include "bios_rtc.h"
#include "bios_serial.h"
#include "bios_storage.h"
#include "selftest_abi.h"

#define SELFTEST_BOOT_PARAMS 0x00090000u
#define SELFTEST_RSDP_LINEAR 0x0009fc00u
#define SELFTEST_ELF_IMAGE_LINEAR 0x00280000u
#define SELFTEST_ELF_IMAGE_CAPACITY 0x00040000u
#define SELFTEST_E820_TABLE_OFF 0x02d0u
#define SELFTEST_E820_MAX 128u

#define ELF32_PT_LOAD 1u

static void cpu_serialize(void) {
    __asm__ volatile("xorl %%eax, %%eax\n\tcpuid"
                     :
                     :
                     : "eax", "ebx", "ecx", "edx", "memory");
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

static unsigned short selftest_le16(const unsigned char* p) {
    return (unsigned short)((unsigned short)p[0] | ((unsigned short)p[1] << 8));
}

static unsigned int selftest_le32(const unsigned char* p) {
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8) |
           ((unsigned int)p[2] << 16) | ((unsigned int)p[3] << 24);
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

static int selftest_is_elf32_i386(const unsigned char* elf) {
    return elf[0] == 0x7fu && elf[1] == 'E' && elf[2] == 'L' &&
           elf[3] == 'F' && elf[4] == 1u && elf[5] == 1u &&
           selftest_le16(elf + 0x10u) == 2u &&
           selftest_le16(elf + 0x12u) == 3u;
}

static int selftest_resolve_load_phys(unsigned int paddr, unsigned int vaddr,
                                      unsigned int* out) {
    if (paddr >= 0x00100000u) {
        *out = paddr;
        return 0;
    }
    if (vaddr >= 0xc0000000u) {
        *out = vaddr - 0xc0000000u;
        return *out >= 0x00100000u ? 0 : -1;
    }
    if (vaddr >= 0x00100000u) {
        *out = vaddr;
        return 0;
    }
    return -1;
}

static int selftest_entry_phys(unsigned int entry, const unsigned char* phdrs,
                               unsigned int phnum, unsigned int phentsize,
                               unsigned int* entry_phys) {
    unsigned int i;

    for (i = 0u; i < phnum; ++i) {
        const unsigned char* ph = phdrs + i * phentsize;
        unsigned int type = selftest_le32(ph + 0u);
        unsigned int vaddr = selftest_le32(ph + 8u);
        unsigned int paddr = selftest_le32(ph + 12u);
        unsigned int memsz = selftest_le32(ph + 20u);
        unsigned int phys;

        if (type != ELF32_PT_LOAD || memsz == 0u ||
            selftest_resolve_load_phys(paddr, vaddr, &phys) != 0) {
            continue;
        }
        if (entry >= phys && entry - phys < memsz) {
            *entry_phys = entry;
            return 0;
        }
        if (entry >= vaddr && entry - vaddr < memsz) {
            *entry_phys = phys + (entry - vaddr);
            return 0;
        }
    }
    return -1;
}

static int selftest_load_elf_image(unsigned char* elf,
                                   unsigned int image_size,
                                   unsigned int total_bytes,
                                   unsigned int* entry_phys) {
    unsigned int entry;
    unsigned int phoff;
    unsigned int phentsize;
    unsigned int phnum;
    unsigned int phdr_bytes;
    unsigned int usable_end = bios_memory_extended_usable_end(total_bytes);
    unsigned int i;

    if (image_size < 52u || !selftest_is_elf32_i386(elf)) {
        serial_write_string("Test ELF bad header\r\n");
        return -1;
    }

    entry = selftest_le32(elf + 0x18u);
    phoff = selftest_le32(elf + 0x1cu);
    phentsize = selftest_le16(elf + 0x2au);
    phnum = selftest_le16(elf + 0x2cu);
    phdr_bytes = phentsize * phnum;
    if (phentsize < 32u || phnum == 0u || phnum > 128u ||
        phdr_bytes > 0x4000u || phoff > image_size ||
        phdr_bytes > image_size - phoff) {
        serial_write_string("Test ELF bad phdr\r\n");
        return -1;
    }

    serial_write_string("Test ELF entry=");
    serial_write_hex32(entry);
    serial_write_string(" phnum=");
    serial_write_u32(phnum);
    serial_write_string("\r\n");

    for (i = 0u; i < phnum; ++i) {
        unsigned char* ph = elf + phoff + i * phentsize;
        unsigned int type = selftest_le32(ph + 0u);
        unsigned int off = selftest_le32(ph + 4u);
        unsigned int vaddr = selftest_le32(ph + 8u);
        unsigned int paddr = selftest_le32(ph + 12u);
        unsigned int filesz = selftest_le32(ph + 16u);
        unsigned int memsz = selftest_le32(ph + 20u);

        if (type != ELF32_PT_LOAD) {
            continue;
        }
        if (selftest_resolve_load_phys(paddr, vaddr, &paddr) != 0 ||
            paddr >= usable_end || filesz > memsz ||
            memsz > usable_end - paddr || off > image_size ||
            filesz > image_size - off) {
            serial_write_string("Test ELF bad LOAD\r\n");
            return -1;
        }

        serial_write_string("Test LOAD ");
        serial_write_hex32(paddr);
        serial_write_string(" filesz=");
        serial_write_hex32(filesz);
        serial_write_string(" memsz=");
        serial_write_hex32(memsz);
        serial_write_string(" off=");
        serial_write_hex32(off);
        serial_write_string("\r\n");

        selftest_memcpy((void*)paddr, elf + off, filesz);
        if (memsz > filesz) {
            selftest_memset((void*)(paddr + filesz), 0u, memsz - filesz);
        }
    }

    if (selftest_entry_phys(entry, elf + phoff, phnum, phentsize,
                            entry_phys) != 0) {
        serial_write_string("Test ELF entry not loaded\r\n");
        return -1;
    }
    return 0;
}

static void selftest_prepare_platform(const struct bios_stage_context* stage) {
    bios_rtc_prepare_for_linux(bios_nvram_enable_extended_cmos);
    bios_acpi_install_for_linux(
        stage->rsdp_linear, stage->acpi_pm1_evt, stage->acpi_pm1_cnt,
        stage->acpi_gpe0, stage->acpi_gpe0_len, stage->acpi_flags);
}

static void selftest_prepare_boot_params(unsigned int total_bytes,
                                         unsigned int entry_phys) {
    unsigned char* bp = (unsigned char*)SELFTEST_BOOT_PARAMS;
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

void selftest_stage3_run_elf_payload(
    const struct bios_stage_context* stage,
    const struct bios_settings* settings) {
    struct app_boot_context* info = app_boot_context_alloc(stage);
    blob_load_fn load = bios_stage_context_blob_load(stage);
    struct blob_status status;
    unsigned char* image = (unsigned char*)SELFTEST_ELF_IMAGE_LINEAR;
    unsigned int load_addr = SELFTEST_ELF_IMAGE_LINEAR;
    unsigned int entry_phys = 0u;
    unsigned int rc;
    int expand_rc;

    if (stage->test_elf_payload_linear == 0u || info == 0) {
        serial_write_string("No test ELF payload\r\n");
        return;
    }
    if (load == 0) {
        serial_write_string("Test ELF payload service missing\r\n");
        return;
    }

    serial_write_string("Run ROM test ELF...\r\n");
    expand_rc = load(SHARED_PAYLOAD_ID_TEST_ELF, image,
                     SELFTEST_ELF_IMAGE_CAPACITY, &load_addr, &status,
                     stage->total_bytes);
    if (expand_rc != 0) {
        serial_write_string("Test ELF payload failed rc=");
        serial_write_hex8((unsigned char)expand_rc);
        serial_write_string(" block=");
        serial_write_hex32(status.block);
        serial_write_string("\r\n");
        return;
    }

    if (selftest_load_elf_image(image, status.output_size, stage->total_bytes,
                                &entry_phys) != 0) {
        return;
    }

    storage_scan(stage->total_bytes);
    selftest_prepare_platform(stage);
    selftest_prepare_boot_params(stage->total_bytes, entry_phys);
    app_boot_context_fill(info, APP_BOOT_ID_SELFTEST, stage, settings);
    if (info->platform.acpi_rsdp_linear == 0u) {
        info->platform.acpi_rsdp_linear = SELFTEST_RSDP_LINEAR;
    }
    info->boot_params_linear = SELFTEST_BOOT_PARAMS;
    info->work_linear = SELFTEST_ELF_IMAGE_LINEAR;
    info->work_size = SELFTEST_ELF_IMAGE_CAPACITY;

    serial_write_string("Call test ELF entry=");
    serial_write_hex32(entry_phys);
    serial_write_string(" info=");
    serial_write_hex32((unsigned int)info);
    serial_write_string("\r\n");
    cpu_serialize();
    rc = ((app_entry_fn)entry_phys)(info);
    cpu_serialize();
    serial_write_string("Test ELF returned ");
    serial_write_hex32(rc);
    serial_write_string("\r\n");
}
