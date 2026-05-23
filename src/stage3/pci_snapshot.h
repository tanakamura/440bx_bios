#ifndef STAGE3_PCI_SNAPSHOT_H
#define STAGE3_PCI_SNAPSHOT_H

struct shared_service_table;

int stage3_pci_snapshot_build(unsigned int total_bytes,
                              struct shared_service_table* shared);

#endif
