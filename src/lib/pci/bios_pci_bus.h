#ifndef BIOS_PCI_BUS_H
#define BIOS_PCI_BUS_H

struct shared_boot_context;

void pci_bus_enumerate_and_assign(unsigned int total_bytes,
                                  struct shared_boot_context* boot);

#endif
