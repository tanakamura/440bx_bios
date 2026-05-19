#ifndef BIOS_SERIAL_H
#define BIOS_SERIAL_H

void serial_write_char(char c);
void serial_write_string(const char* s);
void serial_write_hex8(unsigned char value);
void serial_write_hex16(unsigned short value);
void serial_write_hex32(unsigned int value);
void serial_dump_bytes(const char* tag, const unsigned char* data,
                       unsigned int len);
void serial_write_u32(unsigned int value);
void serial_write_fixed2(unsigned int x100);

#endif
