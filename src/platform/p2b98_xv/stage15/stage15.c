#include "blob.h"
#include "post_code.h"
#include "shared_service/service_table.h"

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

static unsigned int crc32_update(unsigned int crc, unsigned char byte) {
    unsigned int i;
    crc ^= byte;
    for (i = 0u; i < 8u; ++i) {
        unsigned int mask = 0u - (crc & 1u);
        crc = (crc >> 1) ^ (0xedb88320u & mask);
    }
    return crc;
}

static unsigned int crc32(const unsigned char* data, unsigned int len) {
    unsigned int crc = 0xffffffffu;
    unsigned int i;
    for (i = 0u; i < len; ++i) {
        crc = crc32_update(crc, data[i]);
    }
    return crc ^ 0xffffffffu;
}

static unsigned char rom_read_stable_u8(const unsigned char* ptr) {
    volatile const unsigned char* p = (volatile const unsigned char*)ptr;
    unsigned char samples[11];
    unsigned int best = 0u;
    unsigned int best_count = 0u;
    unsigned int i;
    unsigned int j;

    for (i = 0u; i < sizeof(samples); ++i) {
        samples[i] = p[0];
    }
    for (i = 0u; i < sizeof(samples); ++i) {
        unsigned int count = 0u;
        for (j = 0u; j < sizeof(samples); ++j) {
            if (samples[j] == samples[i]) {
                ++count;
            }
        }
        if (count > best_count) {
            best = i;
            best_count = count;
        }
    }
    return samples[best];
}

static unsigned int rom_read_stable_u32(unsigned int linear) {
    unsigned int v = 0u;
    v |= (unsigned int)rom_read_stable_u8((const unsigned char*)linear);
    v |= (unsigned int)rom_read_stable_u8((const unsigned char*)(linear + 1u))
         << 8;
    v |= (unsigned int)rom_read_stable_u8((const unsigned char*)(linear + 2u))
         << 16;
    v |= (unsigned int)rom_read_stable_u8((const unsigned char*)(linear + 3u))
         << 24;
    return v;
}

static int find_stage2_raw(unsigned int* rom_linear_out,
                           unsigned int* load_addr_out,
                           unsigned int* size_out,
                           unsigned int* crc_out) {
    unsigned int base = SHARED_ROM_HIGH_BASE;
    unsigned int entry_count;
    unsigned int payload_start;
    unsigned int payload_end;
    unsigned int stage1_start;
    unsigned int directory_bytes;
    unsigned int sum = 0u;
    unsigned int i;

    if (rom_read_stable_u32(base + 0u) != SHARED_ROM_DIRECTORY_MAGIC ||
        rom_read_stable_u32(base + 4u) != SHARED_ROM_DIRECTORY_VERSION ||
        rom_read_stable_u32(base + 8u) != SHARED_ROM_DIRECTORY_HEADER_SIZE ||
        rom_read_stable_u32(base + 12u) != SHARED_ROM_DIRECTORY_ENTRY_SIZE) {
        return -1;
    }

    entry_count = rom_read_stable_u32(base + 16u);
    payload_start = rom_read_stable_u32(base + 20u);
    payload_end = rom_read_stable_u32(base + 24u);
    stage1_start = rom_read_stable_u32(base + 28u);
    directory_bytes =
        SHARED_ROM_DIRECTORY_HEADER_SIZE +
        entry_count * SHARED_ROM_DIRECTORY_ENTRY_SIZE;
    if (entry_count > SHARED_ROM_DIRECTORY_ENTRY_MAX ||
        payload_start < SHARED_ROM_DIRECTORY_HEADER_SIZE ||
        payload_end > SHARED_ROM_SIZE || payload_start > payload_end ||
        stage1_start > SHARED_ROM_SIZE || payload_end > stage1_start ||
        directory_bytes > payload_start || (directory_bytes & 3u) != 0u) {
        return -1;
    }

    for (i = 0u; i < directory_bytes; i += 4u) {
        sum += rom_read_stable_u32(base + i);
    }
    if (sum != 0u) {
        return -1;
    }

    for (i = 0u; i < entry_count; ++i) {
        unsigned int off =
            SHARED_ROM_DIRECTORY_HEADER_SIZE +
            i * SHARED_ROM_DIRECTORY_ENTRY_SIZE;
        unsigned int id = rom_read_stable_u32(base + off + 0u);
        unsigned int type = rom_read_stable_u32(base + off + 4u);
        unsigned int rom_offset = rom_read_stable_u32(base + off + 12u);
        unsigned int blob_size = rom_read_stable_u32(base + off + 16u);
        unsigned int slot_size = rom_read_stable_u32(base + off + 20u);
        unsigned int load_addr = rom_read_stable_u32(base + off + 24u);
        unsigned int crc = rom_read_stable_u32(base + off + 28u);

        if (id != SHARED_PAYLOAD_ID_STAGE2) {
            continue;
        }
        if (type != SHARED_PAYLOAD_TYPE_RAW || blob_size == 0u ||
            rom_offset < payload_start || rom_offset > payload_end ||
            blob_size > payload_end - rom_offset || slot_size < blob_size ||
            load_addr != STAGE2_LOAD_LINEAR ||
            blob_size > STAGE2_LOAD_CAPACITY) {
            return -1;
        }
        *rom_linear_out = base + rom_offset;
        *load_addr_out = load_addr;
        *size_out = blob_size;
        *crc_out = crc;
        return 0;
    }
    return -1;
}

static void load_stage2_raw(unsigned int total_bytes) {
    typedef void (*stage2_entry_fn)(unsigned int);
    unsigned int rom_linear = 0u;
    unsigned int load_addr = 0u;
    unsigned int size = 0u;
    unsigned int expected_crc = 0u;
    unsigned char* dst;
    unsigned int retry;
    unsigned int got = 0u;

    if (find_stage2_raw(&rom_linear, &load_addr, &size, &expected_crc) != 0) {
        serial_write_string("stage2 raw missing\r\n");
        die_with_post(0xef);
    }

    serial_write_string("Load stage2 raw @ ");
    serial_write_hex32(load_addr);
    serial_write_string("...\r\n");
    dst = (unsigned char*)load_addr;
    for (retry = 0u; retry < 32u; ++retry) {
        unsigned int i;
        serial_write_char('{');
        for (i = 0u; i < size; ++i) {
            if ((i & 0x03ffu) == 0u) {
                serial_write_char('+');
            }
            dst[i] = rom_read_stable_u8((const unsigned char*)(rom_linear + i));
        }
        serial_write_char('}');
        got = crc32(dst, size);
        if (got == expected_crc) {
            break;
        }
        serial_write_char('x');
    }
    if (retry == 32u) {
        serial_write_string("\r\nstage2 raw crc bad exp=");
        serial_write_hex32(expected_crc);
        serial_write_string(" got=");
        serial_write_hex32(got);
        serial_write_string("\r\n");
        die_with_post(0xef);
    }
    serial_write_string("\r\nstage2 raw ok\r\n");
    __asm__ volatile("xorl %%eax, %%eax\n\tcpuid"
                     :
                     :
                     : "eax", "ebx", "ecx", "edx", "memory");
    ((stage2_entry_fn)load_addr)(total_bytes);
    die_with_post(0xef);
}

void stage15_main(unsigned int total_bytes) {
    serial_write_string("stage1.5 @ ");
    serial_write_hex32(STAGE15_LOAD_LINEAR);
    serial_write_string("\r\n");
    load_stage2_raw(total_bytes);
}
