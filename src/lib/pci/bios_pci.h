#ifndef BIOS_PCI_H
#define BIOS_PCI_H

unsigned int pci_read32(unsigned char bus, unsigned char device,
                        unsigned char function, unsigned char reg);
void pci_write32(unsigned char bus, unsigned char device,
                 unsigned char function, unsigned char reg, unsigned int val);
unsigned short pci_read16(unsigned char bus, unsigned char device,
                          unsigned char function, unsigned char reg);
void pci_write16(unsigned char bus, unsigned char device, unsigned char function,
                 unsigned char reg, unsigned short val);
void pci_write8(unsigned char bus, unsigned char device, unsigned char function,
                unsigned char reg, unsigned char val);
unsigned char pci_read8(unsigned char bus, unsigned char device,
                        unsigned char function, unsigned char reg);

#endif
