#include "bios_io.h"
#include "bios_serial.h"

void serial_write_char(char c) {
    while ((inb(0x03f8 + 5) & 0x20) == 0) {
    }
    outb(0x03f8, (unsigned char)c);
}

void serial_write_string(const char* s) {
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

void serial_write_hex8(unsigned char value) {
    serial_write_hex4((unsigned char)(value >> 4));
    serial_write_hex4(value);
}

void serial_write_hex16(unsigned short value) {
    serial_write_hex8((unsigned char)(value >> 8));
    serial_write_hex8((unsigned char)value);
}

void serial_write_hex32(unsigned int value) {
    serial_write_hex16((unsigned short)(value >> 16));
    serial_write_hex16((unsigned short)value);
}

void serial_dump_bytes(const char* tag, const unsigned char* data,
                       unsigned int len) {
    unsigned int i;
    serial_write_string(tag);
    serial_write_string(" @ ");
    serial_write_hex32((unsigned int)data);
    serial_write_string(":");
    for (i = 0; i < len; ++i) {
        serial_write_char(' ');
        serial_write_hex8(data[i]);
    }
    serial_write_string("\r\n");
}

void serial_write_u32(unsigned int value) {
    char buf[10];
    unsigned int i = 0;

    if (value == 0) {
        serial_write_char('0');
        return;
    }

    while (value != 0 && i < sizeof(buf)) {
        buf[i++] = (char)('0' + (value % 10u));
        value /= 10u;
    }
    while (i != 0) {
        serial_write_char(buf[--i]);
    }
}

void serial_write_fixed2(unsigned int x100) {
    serial_write_u32(x100 / 100u);
    serial_write_char('.');
    serial_write_char((char)('0' + ((x100 / 10u) % 10u)));
    serial_write_char((char)('0' + (x100 % 10u)));
}
