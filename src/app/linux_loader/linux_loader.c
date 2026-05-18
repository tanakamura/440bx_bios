#include "app/linux_loader/linux_loader.h"

#include "bios_memory.h"
#include "bios_nvram.h"
#include "bios_serial.h"
#include "bios_storage.h"

#define LINUX_SECTOR_BUF 0x00080000u
#define LINUX_PHDR_BUF 0x00088000u
#define LINUX_CMDLINE 0x00098000u
#define LINUX_CMDLINE_CAPACITY 2048u
#define LINUX_SERIAL_CMDLINE_TEXT \
    "console=ttyS0,115200n8 earlyprintk=serial,ttyS0,115200"
#define LINUX_ELF_PHDR_MAX 0x4000u
#define LINUX_E820_TABLE_OFF 0x02d0u
#define LINUX_E820_MAX 128u
#define ELF32_PT_LOAD 1u
#define VBE_MODE_1024_768_16 0x0117u
#define VBE_MODE_LFB 0x4000u
#define VBE_SUCCESS 0x004fu
#define VBE_ATTR_SUPPORTED 0x0001u
#define VBE_ATTR_GRAPHICS 0x0010u
#define VBE_ATTR_LFB 0x0080u
#define LINUX_VIDEO_TYPE_VLFB 0x23u

struct linux_partition {
    unsigned char boot;
    unsigned char type;
    unsigned int start_lba;
    unsigned int sectors;
};

struct linux_vbe_lfb {
    unsigned char ready;
    unsigned short width;
    unsigned short height;
    unsigned short depth;
    unsigned short pitch;
    unsigned int base;
    unsigned char red_size;
    unsigned char red_pos;
    unsigned char green_size;
    unsigned char green_pos;
    unsigned char blue_size;
    unsigned char blue_pos;
    unsigned char rsvd_size;
    unsigned char rsvd_pos;
    unsigned short pages;
    unsigned short attrs;
};

static struct linux_vbe_lfb linux_vbe;

static unsigned int tsc_low(void) {
    unsigned int value;
    __asm__ volatile("rdtsc" : "=a"(value) : : "edx");
    return value;
}

static void cache_writeback_invalidate(void) {
    __asm__ volatile("wbinvd" : : : "memory");
}

static void cpu_serialize(void) {
    __asm__ volatile("xorl %%eax, %%eax\n\tcpuid"
                     :
                     :
                     : "eax", "ebx", "ecx", "edx", "memory");
}

static void linux_memset(void* dst, unsigned char value, unsigned int len) {
    unsigned char* p = (unsigned char*)dst;
    while (len-- != 0u) {
        *p++ = value;
    }
}

static void linux_memcpy(void* dst, const void* src, unsigned int len) {
    unsigned char* d = (unsigned char*)dst;
    const unsigned char* s = (const unsigned char*)src;
    while (len-- != 0u) {
        *d++ = *s++;
    }
}

static unsigned int linux_usable_end(const struct linux_loader_config* config) {
    return bios_memory_extended_usable_end(config->total_bytes);
}

static unsigned short linux_le16(const unsigned char* p) {
    return (unsigned short)((unsigned short)p[0] | ((unsigned short)p[1] << 8));
}

static unsigned int linux_le32(const unsigned char* p) {
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8) |
           ((unsigned int)p[2] << 16) | ((unsigned int)p[3] << 24);
}

static void linux_put16(unsigned char* p, unsigned short value) {
    p[0] = (unsigned char)value;
    p[1] = (unsigned char)(value >> 8);
}

static void linux_put32(unsigned char* p, unsigned int value) {
    p[0] = (unsigned char)value;
    p[1] = (unsigned char)(value >> 8);
    p[2] = (unsigned char)(value >> 16);
    p[3] = (unsigned char)(value >> 24);
}

static unsigned char* vbe_mode_info(
    const struct linux_loader_config* config) {
    if (config->vbe_mode_info_buffer == 0) {
        return 0;
    }
    return config->vbe_mode_info_buffer();
}

static unsigned short vbe_info16(const struct linux_loader_config* config,
                                 unsigned int off) {
    unsigned char* info = vbe_mode_info(config);
    return (unsigned short)((unsigned short)info[off] |
                            ((unsigned short)info[off + 1u] << 8));
}

static unsigned int vbe_info32(const struct linux_loader_config* config,
                               unsigned int off) {
    unsigned char* info = vbe_mode_info(config);
    return (unsigned int)info[off] | ((unsigned int)info[off + 1u] << 8) |
           ((unsigned int)info[off + 2u] << 16) |
           ((unsigned int)info[off + 3u] << 24);
}

static int vbe_query_mode(const struct linux_loader_config* config,
                          unsigned short mode) {
    unsigned char* info = vbe_mode_info(config);
    unsigned int i;
    unsigned int status;

    if (info == 0 || config->vbe_mode_info_pm32 == 0) {
        return -1;
    }
    for (i = 0u; i < 256u; ++i) {
        info[i] = 0u;
    }
    cache_writeback_invalidate();
    status = config->vbe_mode_info_pm32(mode);
    cache_writeback_invalidate();
    if ((unsigned short)status != VBE_SUCCESS) {
        serial_write_string("VBE 4F01 failed mode=");
        serial_write_hex16(mode);
        serial_write_string(" ax=");
        serial_write_hex16((unsigned short)status);
        serial_write_string("\r\n");
        return -1;
    }
    return 0;
}

static int vbe_set_mode(const struct linux_loader_config* config,
                        unsigned short mode) {
    unsigned int status;

    if (config->vbe_set_mode_pm32 == 0) {
        return -1;
    }
    cache_writeback_invalidate();
    status = config->vbe_set_mode_pm32(mode);
    cache_writeback_invalidate();
    if ((unsigned short)status != VBE_SUCCESS) {
        serial_write_string("VBE 4F02 failed mode=");
        serial_write_hex16(mode);
        serial_write_string(" ax=");
        serial_write_hex16((unsigned short)status);
        serial_write_string("\r\n");
        return -1;
    }
    return 0;
}

static int linux_set_vbe_1024x768(
    const struct linux_loader_config* config) {
    unsigned short attrs;
    unsigned short width;
    unsigned short height;
    unsigned short pitch;
    unsigned char depth;
    unsigned int base;
    unsigned short mode = VBE_MODE_1024_768_16;
    unsigned char* info;

    linux_vbe.ready = 0u;
    if (config->enable_vesa_1024_768 == 0u ||
        vbe_query_mode(config, mode) != 0) {
        return -1;
    }

    info = vbe_mode_info(config);
    attrs = vbe_info16(config, 0x00u);
    width = vbe_info16(config, 0x12u);
    height = vbe_info16(config, 0x14u);
    pitch = vbe_info16(config, 0x10u);
    depth = info[0x19u];
    base = vbe_info32(config, 0x28u);
    if ((attrs & (VBE_ATTR_SUPPORTED | VBE_ATTR_GRAPHICS | VBE_ATTR_LFB)) !=
            (VBE_ATTR_SUPPORTED | VBE_ATTR_GRAPHICS | VBE_ATTR_LFB) ||
        width != 1024u || height != 768u || depth != 16u || pitch == 0u ||
        base == 0u) {
        serial_write_string("VBE 1024x768x16 unusable attrs=");
        serial_write_hex16(attrs);
        serial_write_string(" wh=");
        serial_write_u32(width);
        serial_write_string("x");
        serial_write_u32(height);
        serial_write_string(" depth=");
        serial_write_u32(depth);
        serial_write_string(" base=");
        serial_write_hex32(base);
        serial_write_string("\r\n");
        return -1;
    }

    if (vbe_set_mode(config, (unsigned short)(mode | VBE_MODE_LFB)) != 0) {
        return -1;
    }

    linux_vbe.ready = 1u;
    linux_vbe.width = width;
    linux_vbe.height = height;
    linux_vbe.depth = depth;
    linux_vbe.pitch = pitch;
    linux_vbe.base = base;
    linux_vbe.red_size = info[0x1fu];
    linux_vbe.red_pos = info[0x20u];
    linux_vbe.green_size = info[0x21u];
    linux_vbe.green_pos = info[0x22u];
    linux_vbe.blue_size = info[0x23u];
    linux_vbe.blue_pos = info[0x24u];
    linux_vbe.rsvd_size = info[0x25u];
    linux_vbe.rsvd_pos = info[0x26u];
    linux_vbe.pages = info[0x1du];
    linux_vbe.attrs = attrs;

    serial_write_string("VBE mode 1024x768x16 LFB @ ");
    serial_write_hex32(base);
    serial_write_string(" pitch=");
    serial_write_u32(pitch);
    serial_write_string("\r\n");
    return 0;
}

static void linux_apply_vbe_screen_info(void) {
    unsigned char* bp = (unsigned char*)LINUX_LOADER_BOOT_PARAMS;
    unsigned int lfb_size;

    if (linux_vbe.ready == 0u) {
        return;
    }
    lfb_size = (unsigned int)linux_vbe.pitch * (unsigned int)linux_vbe.height;

    bp[0x006u] = (unsigned char)VBE_MODE_1024_768_16;
    bp[0x00fu] = LINUX_VIDEO_TYPE_VLFB;
    linux_put16(bp + 0x012u, linux_vbe.width);
    linux_put16(bp + 0x014u, linux_vbe.height);
    linux_put16(bp + 0x016u, linux_vbe.depth);
    linux_put32(bp + 0x018u, linux_vbe.base);
    linux_put32(bp + 0x01cu, lfb_size);
    linux_put16(bp + 0x024u, linux_vbe.pitch);
    bp[0x026u] = linux_vbe.red_size;
    bp[0x027u] = linux_vbe.red_pos;
    bp[0x028u] = linux_vbe.green_size;
    bp[0x029u] = linux_vbe.green_pos;
    bp[0x02au] = linux_vbe.blue_size;
    bp[0x02bu] = linux_vbe.blue_pos;
    bp[0x02cu] = linux_vbe.rsvd_size;
    bp[0x02du] = linux_vbe.rsvd_pos;
    linux_put16(bp + 0x032u, linux_vbe.pages);
    linux_put16(bp + 0x034u, linux_vbe.attrs);
}

static int linux_sector_is_elf32_i386(const unsigned char* sector) {
    return sector[0] == 0x7fu && sector[1] == 'E' && sector[2] == 'L' &&
           sector[3] == 'F' && sector[4] == 1u && sector[5] == 1u &&
           linux_le16(sector + 0x12u) == 3u;
}

static int linux_use_whole_disk(struct linux_partition* part) {
    struct bios_hdd_geometry geometry;

    if (!bios_hdd_is_present()) {
        return -1;
    }
    bios_hdd_get_geometry(&geometry);
    if (geometry.total_sectors == 0u) {
        return -1;
    }
    part->boot = 0x00u;
    part->type = 0xffu;
    part->start_lba = 0u;
    part->sectors = geometry.total_sectors;
    return 0;
}

static int linux_parse_mbr_partition(unsigned int index,
                                     struct linux_partition* part,
                                     const unsigned char* mbr) {
    const unsigned char* entry;

    if (index >= 4u || mbr[0x01feu] != 0x55u || mbr[0x01ffu] != 0xaau) {
        return -1;
    }

    entry = mbr + 0x01beu + index * 16u;
    part->boot = entry[0];
    part->type = entry[4];
    part->start_lba = linux_le32(entry + 8u);
    part->sectors = linux_le32(entry + 12u);
    if (part->type == 0u || part->start_lba == 0u || part->sectors == 0u) {
        return -1;
    }
    return 0;
}

static int linux_read_partition(unsigned int index,
                                struct linux_partition* part) {
    unsigned char* mbr = (unsigned char*)LINUX_SECTOR_BUF;

    if (!bios_hdd_is_present() || index >= 4u ||
        bios_hdd_read_sectors(0u, 1u, LINUX_SECTOR_BUF) != 0) {
        return -1;
    }
    return linux_parse_mbr_partition(index, part, mbr);
}

static int linux_select_kernel_source(
    const struct linux_loader_config* config, struct linux_partition* part,
    unsigned char* whole_disk) {
    unsigned char* sector0 = (unsigned char*)LINUX_SECTOR_BUF;

    *whole_disk = 0u;
    if (!bios_hdd_is_present() ||
        bios_hdd_read_sectors(0u, 1u, LINUX_SECTOR_BUF) != 0) {
        return -1;
    }
    if (linux_sector_is_elf32_i386(sector0)) {
        if (linux_use_whole_disk(part) != 0) {
            return -1;
        }
        *whole_disk = 1u;
        return 0;
    }
    return linux_parse_mbr_partition(config->vmlinux_partition, part, sector0);
}

static int linux_partition_contains(const struct linux_partition* part,
                                    unsigned int offset, unsigned int len) {
    unsigned int end;

    if (len == 0u) {
        return 0;
    }
    end = offset + len - 1u;
    if (end < offset) {
        return -1;
    }
    if ((end >> 9) >= part->sectors) {
        return -1;
    }
    return 0;
}

static int linux_read_partition_bytes(const struct linux_partition* part,
                                      unsigned int offset, unsigned int dest,
                                      unsigned int len) {
    unsigned int original_len = len;
    unsigned int start_tsc = 0u;

    if (linux_partition_contains(part, offset, len) != 0) {
        return -1;
    }
    if (original_len >= 0x00100000u) {
        start_tsc = tsc_low();
    }

    while (len != 0u) {
        unsigned int sector_off = offset & 0x1ffu;
        unsigned int chunk;

        if (sector_off == 0u && (dest & 0x1ffu) == 0u && len >= 512u) {
            unsigned int count = len >> 9;
            if (count > 2048u) {
                count = 2048u;
            }
            if (bios_hdd_read_sectors(part->start_lba + (offset >> 9), count,
                                      dest) != 0) {
                return -1;
            }
            chunk = count << 9;
        } else {
            if (bios_hdd_read_sectors(part->start_lba + (offset >> 9), 1u,
                                      LINUX_SECTOR_BUF) != 0) {
                return -1;
            }
            chunk = 512u - sector_off;
            if (chunk > len) {
                chunk = len;
            }
            linux_memcpy((void*)dest,
                         (const void*)(LINUX_SECTOR_BUF + sector_off), chunk);
        }
        offset += chunk;
        dest += chunk;
        len -= chunk;
    }
    if (original_len >= 0x00100000u) {
        unsigned int cycles = tsc_low() - start_tsc;
        serial_write_string("Linux read bytes=");
        serial_write_hex32(original_len);
        serial_write_string(" cycles=");
        serial_write_hex32(cycles);
        serial_write_string("\r\n");
    }
    return 0;
}

static int linux_elf_entry_phys(const struct linux_loader_config* config,
                                unsigned int entry, unsigned char* phdrs,
                                unsigned int phnum, unsigned int phentsize,
                                unsigned int* entry_phys) {
    unsigned int i;

    for (i = 0; i < phnum; ++i) {
        unsigned char* ph = phdrs + i * phentsize;
        unsigned int type = linux_le32(ph + 0u);
        unsigned int vaddr = linux_le32(ph + 8u);
        unsigned int paddr = linux_le32(ph + 12u);
        unsigned int memsz = linux_le32(ph + 20u);
        if (type == ELF32_PT_LOAD && entry >= vaddr && entry - vaddr < memsz) {
            *entry_phys = paddr + (entry - vaddr);
            return 0;
        }
    }
    if (entry >= 0x00100000u && entry < linux_usable_end(config)) {
        *entry_phys = entry;
        return 0;
    }
    return -1;
}

static int linux_resolve_load_phys(unsigned int paddr, unsigned int vaddr,
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

static int linux_load_initrd(const struct linux_loader_config* config,
                             const struct linux_partition* part,
                             unsigned int load_high, unsigned int* initrd_base,
                             unsigned int* initrd_size) {
    unsigned int size;
    unsigned int base;
    unsigned int usable_end = linux_usable_end(config);

    if (part->sectors > (usable_end >> 9)) {
        return -1;
    }
    size = part->sectors << 9;
    if (size == 0u || size > (usable_end - 0x00100000u)) {
        return -1;
    }
    base = (usable_end - size) & ~0xfffu;
    if (base < 0x00100000u || base < load_high || base + size > usable_end) {
        return -1;
    }
    if (linux_read_partition_bytes(part, 0u, base, size) != 0) {
        return -1;
    }
    *initrd_base = base;
    *initrd_size = size;
    return 0;
}

static void linux_write_cmdline(const struct linux_loader_config* config) {
    unsigned int i;
    unsigned char* dst = (unsigned char*)LINUX_CMDLINE;

    i = 0u;
    if (config->enable_serial_console != 0u) {
        static const char serial_text[] = LINUX_SERIAL_CMDLINE_TEXT;
        unsigned int j;
        for (j = 0u; serial_text[j] != '\0' &&
                    i + 1u < LINUX_CMDLINE_CAPACITY;
             ++j) {
            dst[i++] = (unsigned char)serial_text[j];
        }
    }
    if (config->cmdline_suffix != 0 && config->cmdline_suffix[0] != '\0' &&
        i != 0u && i + 1u < LINUX_CMDLINE_CAPACITY) {
        dst[i++] = ' ';
    }
    if (config->cmdline_suffix != 0 && config->cmdline_suffix[0] != '\0') {
        unsigned int j;
        for (j = 0; config->cmdline_suffix[j] != '\0' &&
                    i + 1u < LINUX_CMDLINE_CAPACITY;
             ++j) {
            dst[i++] = (unsigned char)config->cmdline_suffix[j];
        }
    }
    dst[i] = '\0';
}

static void linux_setup_boot_params(
    const struct linux_loader_config* config, unsigned int entry_phys,
    unsigned int initrd_base, unsigned int initrd_size) {
    unsigned char* bp = (unsigned char*)LINUX_LOADER_BOOT_PARAMS;
    unsigned int count = bios_memory_e820_entry_count(config->total_bytes);
    unsigned int i;
    unsigned int alt_mem_kb = 0u;
    unsigned int usable_end = linux_usable_end(config);

    if (count > LINUX_E820_MAX) {
        count = LINUX_E820_MAX;
    }
    if (usable_end > 0x00100000u) {
        alt_mem_kb = (usable_end - 0x00100000u) >> 10;
        if (alt_mem_kb > 0xffffu) {
            alt_mem_kb = 0xffffu;
        }
    }

    linux_memset(bp, 0u, 4096u);
    linux_write_cmdline(config);

    linux_put16(bp + 0x01e0u, (unsigned short)alt_mem_kb);
    bp[0x01e8u] = (unsigned char)count;
    for (i = 0; i < count; ++i) {
        struct e820_entry entry;
        if (bios_memory_e820_get_entry(config->total_bytes, i, &entry) != 0) {
            break;
        }
        linux_memcpy(bp + LINUX_E820_TABLE_OFF + i * sizeof(entry), &entry,
                     sizeof(entry));
    }

    linux_put16(bp + 0x01feu, 0xaa55u);
    linux_put32(bp + 0x0202u, 0x53726448u);
    linux_put16(bp + 0x0206u, 0x020fu);
    bp[0x0210u] = 0xffu;
    bp[0x0211u] = 0x80u;
    linux_put16(bp + 0x0224u, 0xe000u);
    linux_put32(bp + 0x0214u, entry_phys);
    linux_put32(bp + 0x0218u, initrd_base);
    linux_put32(bp + 0x021cu, initrd_size);
    linux_put32(bp + 0x0228u, LINUX_CMDLINE);
    linux_put32(bp + 0x022cu, usable_end - 1u);
    linux_put32(bp + 0x0230u, 0x00100000u);
    bp[0x0234u] = 0u;
    linux_put32(bp + 0x0238u, LINUX_CMDLINE_CAPACITY);
}

void linux_loader_prepare_boot_params(
    const struct linux_loader_config* config, unsigned int entry_phys,
    unsigned int initrd_base, unsigned int initrd_size) {
    if (config->prepare_platform != 0) {
        config->prepare_platform();
    }
    linux_setup_boot_params(config, entry_phys, initrd_base, initrd_size);
    if (config->init_vgabios != 0) {
        config->init_vgabios();
    }
    if (linux_set_vbe_1024x768(config) == 0) {
        linux_apply_vbe_screen_info();
    }
}

static void linux_jump(unsigned int entry_phys) {
    cpu_serialize();
    __asm__ volatile(
        "cli\n\t"
        "cld\n\t"
        "movl %0, %%esi\n\t"
        "xorl %%ebp, %%ebp\n\t"
        "jmp *%1"
        :
        : "r"(LINUX_LOADER_BOOT_PARAMS), "r"(entry_phys)
        : "esi", "ebp", "memory");
    for (;;) {
        __asm__ volatile("hlt");
    }
}

int linux_loader_load_elf_image(const struct linux_loader_config* config,
                                unsigned char* elf,
                                unsigned int image_size,
                                unsigned int* entry_phys) {
    unsigned int entry;
    unsigned int phoff;
    unsigned int phentsize;
    unsigned int phnum;
    unsigned int phdr_bytes;
    unsigned int i;
    unsigned int usable_end = linux_usable_end(config);

    if (image_size < 52u || !linux_sector_is_elf32_i386(elf)) {
        serial_write_string("Test ELF bad header\r\n");
        return -1;
    }

    entry = linux_le32(elf + 0x18u);
    phoff = linux_le32(elf + 0x1cu);
    phentsize = linux_le16(elf + 0x2au);
    phnum = linux_le16(elf + 0x2cu);
    phdr_bytes = phentsize * phnum;
    if (phentsize < 32u || phnum == 0u || phnum > 128u ||
        phdr_bytes > LINUX_ELF_PHDR_MAX || phoff > image_size ||
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
        unsigned int type = linux_le32(ph + 0u);
        unsigned int off = linux_le32(ph + 4u);
        unsigned int vaddr = linux_le32(ph + 8u);
        unsigned int paddr = linux_le32(ph + 12u);
        unsigned int filesz = linux_le32(ph + 16u);
        unsigned int memsz = linux_le32(ph + 20u);

        if (type != ELF32_PT_LOAD) {
            continue;
        }
        if (linux_resolve_load_phys(paddr, vaddr, &paddr) != 0 ||
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

        linux_memcpy((void*)paddr, elf + off, filesz);
        if (memsz > filesz) {
            linux_memset((void*)(paddr + filesz), 0u, memsz - filesz);
        }
    }

    if (linux_elf_entry_phys(config, entry, elf + phoff, phnum, phentsize,
                             entry_phys) != 0) {
        serial_write_string("Test ELF entry not loaded\r\n");
        return -1;
    }
    return 0;
}

static int try_boot_linux_current(const struct linux_loader_config* config) {
    struct linux_partition kernel_part;
    struct linux_partition initrd_part;
    unsigned char* ehdr = (unsigned char*)LINUX_SECTOR_BUF;
    unsigned char* phdrs = (unsigned char*)LINUX_PHDR_BUF;
    unsigned int entry;
    unsigned int entry_phys = 0u;
    unsigned int phoff;
    unsigned int phentsize;
    unsigned int phnum;
    unsigned int phdr_bytes;
    unsigned int load_high = 0x00100000u;
    unsigned int initrd_base = 0u;
    unsigned int initrd_size = 0u;
    unsigned int i;
    unsigned int usable_end = linux_usable_end(config);
    unsigned char whole_disk = 0u;

    if (linux_select_kernel_source(config, &kernel_part, &whole_disk) != 0) {
        return 0;
    }
    if (whole_disk) {
        serial_write_string("Linux disk start=");
    } else {
        serial_write_string("Linux part");
        serial_write_u32((unsigned int)config->vmlinux_partition + 1u);
        serial_write_string(" start=");
    }
    serial_write_hex32(kernel_part.start_lba);
    serial_write_string(" size=");
    serial_write_hex32(kernel_part.sectors);
    if (!whole_disk) {
        serial_write_string(" type=");
        serial_write_hex8(kernel_part.type);
    }
    serial_write_string("\r\n");

    if (linux_read_partition_bytes(&kernel_part, 0u, LINUX_SECTOR_BUF, 512u) !=
        0) {
        return 0;
    }
    if (!linux_sector_is_elf32_i386(ehdr)) {
        serial_write_string("Linux kernel is not ELF32 i386\r\n");
        return 0;
    }

    entry = linux_le32(ehdr + 0x18u);
    phoff = linux_le32(ehdr + 0x1cu);
    phentsize = linux_le16(ehdr + 0x2au);
    phnum = linux_le16(ehdr + 0x2cu);
    phdr_bytes = phentsize * phnum;
    if (phentsize < 32u || phnum == 0u || phnum > 128u ||
        phdr_bytes > LINUX_ELF_PHDR_MAX ||
        linux_read_partition_bytes(&kernel_part, phoff, LINUX_PHDR_BUF,
                                   phdr_bytes) != 0) {
        serial_write_string("Linux bad ELF phdr\r\n");
        return 0;
    }

    serial_write_string("Linux ELF entry=");
    serial_write_hex32(entry);
    serial_write_string(" phnum=");
    serial_write_u32(phnum);
    serial_write_string("\r\n");

    for (i = 0; i < phnum; ++i) {
        unsigned char* ph = phdrs + i * phentsize;
        unsigned int type = linux_le32(ph + 0u);
        unsigned int off = linux_le32(ph + 4u);
        unsigned int vaddr = linux_le32(ph + 8u);
        unsigned int paddr = linux_le32(ph + 12u);
        unsigned int filesz = linux_le32(ph + 16u);
        unsigned int memsz = linux_le32(ph + 20u);

        if (type != ELF32_PT_LOAD) {
            continue;
        }
        if (linux_resolve_load_phys(paddr, vaddr, &paddr) != 0 ||
            paddr >= usable_end || filesz > memsz ||
            memsz > usable_end - paddr ||
            linux_partition_contains(&kernel_part, off, filesz) != 0) {
            serial_write_string("Linux bad LOAD\r\n");
            return 0;
        }
        linux_put32(ph + 12u, paddr);

        serial_write_string("Linux LOAD ");
        serial_write_hex32(paddr);
        serial_write_string(" filesz=");
        serial_write_hex32(filesz);
        serial_write_string(" memsz=");
        serial_write_hex32(memsz);
        serial_write_string(" off=");
        serial_write_hex32(off);
        serial_write_string("\r\n");

        if (linux_read_partition_bytes(&kernel_part, off, paddr, filesz) != 0) {
            serial_write_string("Linux LOAD read failed\r\n");
            return 0;
        }
        if (memsz > filesz) {
            linux_memset((void*)(paddr + filesz), 0u, memsz - filesz);
        }
        if (paddr + memsz > load_high) {
            load_high = paddr + memsz;
        }
    }

    if (linux_elf_entry_phys(config, entry, phdrs, phnum, phentsize,
                             &entry_phys) != 0) {
        serial_write_string("Linux entry not loaded\r\n");
        return 0;
    }

    if (!whole_disk && config->vmlinux_partition != 1u &&
        linux_read_partition(1u, &initrd_part) == 0 &&
        linux_load_initrd(config, &initrd_part, load_high, &initrd_base,
                          &initrd_size) == 0) {
        serial_write_string("Linux initrd @ ");
        serial_write_hex32(initrd_base);
        serial_write_string(" size=");
        serial_write_hex32(initrd_size);
        serial_write_string("\r\n");
    } else {
        serial_write_string("Linux initrd: none\r\n");
    }

    linux_loader_prepare_boot_params(config, entry_phys, initrd_base,
                                     initrd_size);
    if (config->record_boot_success != 0) {
        config->record_boot_success(bios_hdd_current_kind());
    }
    serial_write_string("Boot Linux entry=");
    serial_write_hex32(entry_phys);
    serial_write_string(" params=");
    serial_write_hex32(LINUX_LOADER_BOOT_PARAMS);
    serial_write_string("\r\n");
    linux_jump(entry_phys);
    return 1;
}

int linux_loader_try_boot(const struct linux_loader_config* config) {
    if (config->boot_priority == BIOS_NVRAM_BOOT_PRIORITY_IDE) {
        if (bios_hdd_select_kind(BIOS_HDD_KIND_IDE) == 0u) {
            return 0;
        }
        return try_boot_linux_current(config);
    }
    if (config->boot_priority == BIOS_NVRAM_BOOT_PRIORITY_USB) {
        if (bios_hdd_select_kind(BIOS_HDD_KIND_USB) == 0u) {
            return 0;
        }
        return try_boot_linux_current(config);
    }

    if (bios_hdd_select_kind(BIOS_HDD_KIND_IDE) != 0u &&
        try_boot_linux_current(config)) {
        return 1;
    }
    if (bios_hdd_select_kind(BIOS_HDD_KIND_USB) != 0u &&
        try_boot_linux_current(config)) {
        return 1;
    }
    return 0;
}
