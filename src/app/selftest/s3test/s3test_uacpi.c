#include <uacpi/sleep.h>
#include <uacpi/status.h>
#include <uacpi/uacpi.h>

#define S3TEST_HEAP_BASE 0x00280000u
#define S3TEST_HEAP_LIMIT 0x00370000u

static unsigned int s3test_rsdp = 0;
static unsigned int heap_next = S3TEST_HEAP_BASE;
static unsigned int mono_ns = 0;

static inline void outb(unsigned short port, unsigned char value) {
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline unsigned char inb(unsigned short port) {
    unsigned char value;
    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline void outw(unsigned short port, unsigned short value) {
    __asm__ volatile("outw %0, %1" : : "a"(value), "Nd"(port));
}

static inline unsigned short inw(unsigned short port) {
    unsigned short value;
    __asm__ volatile("inw %1, %0" : "=a"(value) : "Nd"(port));
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

static void serial_write_char(char ch) {
    while ((inb(0x03f8u + 5u) & 0x20u) == 0) {
    }
    outb(0x03f8u, (unsigned char)ch);
}

static void serial_write_string(const char* s) {
    while (*s != '\0') {
        serial_write_char(*s++);
    }
}

static void serial_write_hex4(unsigned char value) {
    value &= 0x0fu;
    if (value < 10u) {
        serial_write_char((char)('0' + value));
    } else {
        serial_write_char((char)('a' + (value - 10u)));
    }
}

static void serial_write_hex32(unsigned int value) {
    unsigned int shift;
    for (shift = 28u; shift != 0xffffffffu; shift -= 4u) {
        serial_write_hex4((unsigned char)(value >> shift));
        if (shift == 0u) {
            break;
        }
    }
}

static void serial_write_status(const char* name, uacpi_status st) {
    serial_write_string(name);
    serial_write_string("=");
    serial_write_hex32((unsigned int)st);
    serial_write_string("\r\n");
}

static unsigned int pci_addr(unsigned char bus, unsigned char dev,
                             unsigned char fn, unsigned char off) {
    return 0x80000000u | ((unsigned int)bus << 16) |
           ((unsigned int)dev << 11) | ((unsigned int)fn << 8) |
           ((unsigned int)off & 0xfcu);
}

static unsigned int pci_read32_raw(unsigned char bus, unsigned char dev,
                                   unsigned char fn, unsigned char off) {
    outl(0x0cf8u, pci_addr(bus, dev, fn, off));
    return inl(0x0cfcu);
}

static void pci_write32_raw(unsigned char bus, unsigned char dev,
                            unsigned char fn, unsigned char off,
                            unsigned int value) {
    outl(0x0cf8u, pci_addr(bus, dev, fn, off));
    outl(0x0cfcu, value);
}

static unsigned int uacpi_handle_value(uacpi_handle handle) {
    return (unsigned int)(uacpi_uintptr)handle;
}

static unsigned int align_up(unsigned int value, unsigned int align) {
    return (value + align - 1u) & ~(align - 1u);
}

void* memcpy(void* dst, const void* src, uacpi_size len) {
    unsigned char* d = (unsigned char*)dst;
    const unsigned char* s = (const unsigned char*)src;
    while (len-- != 0u) {
        *d++ = *s++;
    }
    return dst;
}

void* memmove(void* dst, const void* src, uacpi_size len) {
    unsigned char* d = (unsigned char*)dst;
    const unsigned char* s = (const unsigned char*)src;
    if (d < s) {
        while (len-- != 0u) {
            *d++ = *s++;
        }
    } else {
        d += len;
        s += len;
        while (len-- != 0u) {
            *--d = *--s;
        }
    }
    return dst;
}

void* memset(void* dst, int value, uacpi_size len) {
    unsigned char* d = (unsigned char*)dst;
    while (len-- != 0u) {
        *d++ = (unsigned char)value;
    }
    return dst;
}

int memcmp(const void* lhs, const void* rhs, uacpi_size len) {
    const unsigned char* a = (const unsigned char*)lhs;
    const unsigned char* b = (const unsigned char*)rhs;
    while (len-- != 0u) {
        if (*a != *b) {
            return (int)*a - (int)*b;
        }
        ++a;
        ++b;
    }
    return 0;
}

uacpi_status uacpi_kernel_get_rsdp(uacpi_phys_addr* out_rsdp_address) {
    *out_rsdp_address = s3test_rsdp;
    return UACPI_STATUS_OK;
}

void* uacpi_kernel_map(uacpi_phys_addr addr, uacpi_size len) {
    (void)len;
    return (void*)(uacpi_uintptr)addr;
}

void uacpi_kernel_unmap(void* addr, uacpi_size len) {
    (void)addr;
    (void)len;
}

void uacpi_kernel_log(uacpi_log_level level, const uacpi_char* msg) {
    (void)level;
    serial_write_string("uACPI: ");
    serial_write_string(msg);
    serial_write_string("\r\n");
}

void* uacpi_kernel_alloc(uacpi_size size) {
    unsigned int ptr = align_up(heap_next, 16u);
    unsigned int next = align_up(ptr + (unsigned int)size, 16u);
    if (next > S3TEST_HEAP_LIMIT || next < ptr) {
        return UACPI_NULL;
    }
    heap_next = next;
    return (void*)ptr;
}

void uacpi_kernel_free(void* mem) { (void)mem; }

uacpi_u64 uacpi_kernel_get_nanoseconds_since_boot(void) {
    mono_ns += 1000000u;
    return mono_ns;
}

void uacpi_kernel_stall(uacpi_u8 usec) {
    volatile unsigned int i;
    for (i = 0u; i < (unsigned int)usec * 32u; ++i) {
    }
}

void uacpi_kernel_sleep(uacpi_u64 msec) {
    while (msec-- != 0u) {
        uacpi_kernel_stall(250u);
        uacpi_kernel_stall(250u);
        uacpi_kernel_stall(250u);
        uacpi_kernel_stall(250u);
    }
}

uacpi_handle uacpi_kernel_create_mutex(void) { return (uacpi_handle)1u; }
void uacpi_kernel_free_mutex(uacpi_handle handle) { (void)handle; }
uacpi_status uacpi_kernel_acquire_mutex(uacpi_handle handle,
                                        uacpi_u16 timeout) {
    (void)handle;
    (void)timeout;
    return UACPI_STATUS_OK;
}
void uacpi_kernel_release_mutex(uacpi_handle handle) { (void)handle; }

uacpi_handle uacpi_kernel_create_event(void) { return (uacpi_handle)2u; }
void uacpi_kernel_free_event(uacpi_handle handle) { (void)handle; }
uacpi_bool uacpi_kernel_wait_for_event(uacpi_handle handle, uacpi_u16 timeout) {
    (void)handle;
    (void)timeout;
    return UACPI_TRUE;
}
void uacpi_kernel_signal_event(uacpi_handle handle) { (void)handle; }
void uacpi_kernel_reset_event(uacpi_handle handle) { (void)handle; }

uacpi_thread_id uacpi_kernel_get_thread_id(void) {
    return (uacpi_thread_id)1u;
}

uacpi_interrupt_state uacpi_kernel_disable_interrupts(void) {
    unsigned int flags;
    __asm__ volatile("pushfl\n\tpopl %0\n\tcli" : "=r"(flags) : : "memory");
    return flags;
}

void uacpi_kernel_restore_interrupts(uacpi_interrupt_state state) {
    __asm__ volatile("pushl %0\n\tpopfl" : : "r"((unsigned int)state) : "memory");
}

uacpi_status uacpi_kernel_pci_device_open(uacpi_pci_address address,
                                          uacpi_handle* out_handle) {
    if (address.segment != 0u || address.device > 31u ||
        address.function > 7u) {
        return UACPI_STATUS_NOT_FOUND;
    }
    *out_handle = (uacpi_handle)(uacpi_uintptr)(
        0x80000000u | ((unsigned int)address.bus << 16) |
        ((unsigned int)address.device << 11) |
        ((unsigned int)address.function << 8));
    return UACPI_STATUS_OK;
}

void uacpi_kernel_pci_device_close(uacpi_handle handle) { (void)handle; }

uacpi_status uacpi_kernel_pci_read8(uacpi_handle device, uacpi_size offset,
                                    uacpi_u8* value) {
    unsigned int h = uacpi_handle_value(device);
    unsigned int v = pci_read32_raw((unsigned char)(h >> 16),
                                    (unsigned char)(h >> 11),
                                    (unsigned char)(h >> 8),
                                    (unsigned char)offset);
    *value = (uacpi_u8)(v >> ((offset & 3u) * 8u));
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_read16(uacpi_handle device, uacpi_size offset,
                                     uacpi_u16* value) {
    unsigned int h = uacpi_handle_value(device);
    unsigned int v = pci_read32_raw((unsigned char)(h >> 16),
                                    (unsigned char)(h >> 11),
                                    (unsigned char)(h >> 8),
                                    (unsigned char)offset);
    *value = (uacpi_u16)(v >> ((offset & 2u) * 8u));
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_read32(uacpi_handle device, uacpi_size offset,
                                     uacpi_u32* value) {
    unsigned int h = uacpi_handle_value(device);
    *value = pci_read32_raw((unsigned char)(h >> 16),
                            (unsigned char)(h >> 11),
                            (unsigned char)(h >> 8), (unsigned char)offset);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_write8(uacpi_handle device, uacpi_size offset,
                                     uacpi_u8 value) {
    unsigned int h = uacpi_handle_value(device);
    unsigned int shift = (offset & 3u) * 8u;
    unsigned int mask = 0xffu << shift;
    unsigned int v = pci_read32_raw((unsigned char)(h >> 16),
                                    (unsigned char)(h >> 11),
                                    (unsigned char)(h >> 8),
                                    (unsigned char)offset);
    v = (v & ~mask) | ((unsigned int)value << shift);
    pci_write32_raw((unsigned char)(h >> 16), (unsigned char)(h >> 11),
                    (unsigned char)(h >> 8), (unsigned char)offset, v);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_write16(uacpi_handle device, uacpi_size offset,
                                      uacpi_u16 value) {
    unsigned int h = uacpi_handle_value(device);
    unsigned int shift = (offset & 2u) * 8u;
    unsigned int mask = 0xffffu << shift;
    unsigned int v = pci_read32_raw((unsigned char)(h >> 16),
                                    (unsigned char)(h >> 11),
                                    (unsigned char)(h >> 8),
                                    (unsigned char)offset);
    v = (v & ~mask) | ((unsigned int)value << shift);
    pci_write32_raw((unsigned char)(h >> 16), (unsigned char)(h >> 11),
                    (unsigned char)(h >> 8), (unsigned char)offset, v);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_write32(uacpi_handle device, uacpi_size offset,
                                      uacpi_u32 value) {
    unsigned int h = uacpi_handle_value(device);
    pci_write32_raw((unsigned char)(h >> 16), (unsigned char)(h >> 11),
                    (unsigned char)(h >> 8), (unsigned char)offset, value);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_map(uacpi_io_addr base, uacpi_size len,
                                 uacpi_handle* out_handle) {
    (void)len;
    *out_handle = (uacpi_handle)(uacpi_uintptr)(base + 1u);
    return UACPI_STATUS_OK;
}

void uacpi_kernel_io_unmap(uacpi_handle handle) { (void)handle; }

uacpi_status uacpi_kernel_io_read8(uacpi_handle handle, uacpi_size offset,
                                   uacpi_u8* out_value) {
    *out_value = inb((unsigned short)(uacpi_handle_value(handle) - 1u + offset));
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_read16(uacpi_handle handle, uacpi_size offset,
                                    uacpi_u16* out_value) {
    *out_value = inw((unsigned short)(uacpi_handle_value(handle) - 1u + offset));
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_read32(uacpi_handle handle, uacpi_size offset,
                                    uacpi_u32* out_value) {
    *out_value = inl((unsigned short)(uacpi_handle_value(handle) - 1u + offset));
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_write8(uacpi_handle handle, uacpi_size offset,
                                    uacpi_u8 value) {
    outb((unsigned short)(uacpi_handle_value(handle) - 1u + offset), value);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_write16(uacpi_handle handle, uacpi_size offset,
                                     uacpi_u16 value) {
    outw((unsigned short)(uacpi_handle_value(handle) - 1u + offset), value);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_write32(uacpi_handle handle, uacpi_size offset,
                                     uacpi_u32 value) {
    outl((unsigned short)(uacpi_handle_value(handle) - 1u + offset), value);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_handle_firmware_request(
    uacpi_firmware_request* request) {
    (void)request;
    return UACPI_STATUS_UNIMPLEMENTED;
}

uacpi_status uacpi_kernel_install_interrupt_handler(
    uacpi_u32 irq, uacpi_interrupt_handler handler, uacpi_handle ctx,
    uacpi_handle* out_irq_handle) {
    (void)irq;
    (void)handler;
    (void)ctx;
    *out_irq_handle = (uacpi_handle)3u;
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_uninstall_interrupt_handler(
    uacpi_interrupt_handler handler, uacpi_handle irq_handle) {
    (void)handler;
    (void)irq_handle;
    return UACPI_STATUS_OK;
}

uacpi_handle uacpi_kernel_create_spinlock(void) { return (uacpi_handle)4u; }
void uacpi_kernel_free_spinlock(uacpi_handle handle) { (void)handle; }
uacpi_cpu_flags uacpi_kernel_lock_spinlock(uacpi_handle handle) {
    (void)handle;
    return uacpi_kernel_disable_interrupts();
}
void uacpi_kernel_unlock_spinlock(uacpi_handle handle, uacpi_cpu_flags flags) {
    (void)handle;
    uacpi_kernel_restore_interrupts(flags);
}

uacpi_status uacpi_kernel_schedule_work(uacpi_work_type type,
                                        uacpi_work_handler handler,
                                        uacpi_handle ctx) {
    (void)type;
    handler(ctx);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_wait_for_work_completion(void) {
    return UACPI_STATUS_OK;
}

unsigned int s3test_run_uacpi(unsigned int rsdp) {
    uacpi_status st;
    uacpi_object* arg0;
    uacpi_object* args_array[1];
    uacpi_object_array args;

    s3test_rsdp = rsdp;
    heap_next = S3TEST_HEAP_BASE;
    mono_ns = 0u;

    serial_write_string("uACPI START heap=");
    serial_write_hex32(S3TEST_HEAP_BASE);
    serial_write_string("-");
    serial_write_hex32(S3TEST_HEAP_LIMIT);
    serial_write_string("\r\n");

    st = uacpi_initialize(UACPI_FLAG_NO_ACPI_MODE | UACPI_FLAG_BAD_XSDT);
    serial_write_status("uacpi_initialize", st);
    if (st != UACPI_STATUS_OK) {
        return (unsigned int)st;
    }

    st = uacpi_namespace_load();
    serial_write_status("uacpi_namespace_load", st);
    if (st != UACPI_STATUS_OK) {
        return (unsigned int)st;
    }

    st = uacpi_namespace_initialize();
    serial_write_status("uacpi_namespace_initialize", st);
    if (st != UACPI_STATUS_OK) {
        return (unsigned int)st;
    }

    arg0 = uacpi_object_create_integer(3u);
    if (arg0 == UACPI_NULL) {
        serial_write_string("uACPI arg alloc failed\r\n");
        return UACPI_STATUS_OUT_OF_MEMORY;
    }
    args_array[0] = arg0;
    args.objects = args_array;
    args.count = 1u;

    st = uacpi_execute(UACPI_NULL, "\\_PTS", &args);
    serial_write_status("uacpi_execute _PTS(3)", st);
    uacpi_object_unref(arg0);
    serial_write_string("uACPI heap_next=");
    serial_write_hex32(heap_next);
    serial_write_string("\r\n");

    return (unsigned int)st;
}
