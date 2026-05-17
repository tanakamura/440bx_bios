#include "bios_io.h"
#include "bios_pci.h"

unsigned int pci_read32(unsigned char bus, unsigned char device,
                        unsigned char function, unsigned char reg) {
    unsigned int address = 0x80000000u | ((unsigned int)bus << 16) |
                           ((unsigned int)device << 11) |
                           ((unsigned int)function << 8) | (reg & 0xfcu);
    outl(0x0cf8, address);
    return inl(0x0cfc);
}

void pci_write32(unsigned char bus, unsigned char device, unsigned char function,
                 unsigned char reg, unsigned int val) {
    unsigned int address = 0x80000000u | ((unsigned int)bus << 16) |
                           ((unsigned int)device << 11) |
                           ((unsigned int)function << 8) | (reg & 0xfcu);
    outl(0x0cf8, address);
    outl(0x0cfc, val);
}

unsigned short pci_read16(unsigned char bus, unsigned char device,
                          unsigned char function, unsigned char reg) {
    unsigned int value = pci_read32(bus, device, function, reg);
    return (unsigned short)(value >> ((reg & 0x02u) * 8u));
}

void pci_write16(unsigned char bus, unsigned char device, unsigned char function,
                 unsigned char reg, unsigned short val) {
    unsigned char reg_lo = reg & 0x02u;
    unsigned int address = 0x80000000u | ((unsigned int)bus << 16) |
                           ((unsigned int)device << 11) |
                           ((unsigned int)function << 8) | (reg & 0xfcu);
    outl(0x0cf8, address);
    outw((unsigned short)(0x0cfc + reg_lo), val);
}

void pci_write8(unsigned char bus, unsigned char device, unsigned char function,
                unsigned char reg, unsigned char val) {
    unsigned char reg_lo = reg & 0x03u;
    unsigned int address = 0x80000000u | ((unsigned int)bus << 16) |
                           ((unsigned int)device << 11) |
                           ((unsigned int)function << 8) | (reg & 0xfcu);
    outl(0x0cf8, address);
    outb((unsigned short)(0x0cfc + reg_lo), val);
}

unsigned char pci_read8(unsigned char bus, unsigned char device,
                        unsigned char function, unsigned char reg) {
    unsigned int value = pci_read32(bus, device, function, reg);
    return (unsigned char)(value >> ((reg & 0x03u) * 8u));
}
