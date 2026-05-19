#ifndef ACPI_TABLES_H
#define ACPI_TABLES_H

#define ACPI_RSDP_LINEAR 0x0009fc00u
#define ACPI_EBDA_SEGMENT 0x9fc0u
#define ACPI_TABLE_RESERVED_OFFSET 0x00080000u
#define ACPI_LOW_TABLE_LINEAR 0x000d0000u
#define ACPI_LOW_TABLE_CAPACITY 0x00010000u

#define QEMU_ACPI_PM_BASE 0x0000b000u
#define ACPI_REAL_PM1_EVT 0x0000e400u
#define ACPI_REAL_PM1_CNT 0x0000e404u
#define ACPI_REAL_GPE0 0x0000e40cu
#define ACPI_GPE0_LEN 4u
#define ACPI_REAL_PCI_DEV 7u
#define ACPI_REAL_PCI_FN 3u
#define ACPI_PM1_CNT_SCI_EN 0x0001u

unsigned int acpi_table_base_for_total(unsigned int total_bytes);
unsigned int acpi_table_capacity_for_total(unsigned int total_bytes);
void acpi_build_real_tables(unsigned int base, unsigned int dsdt,
                            unsigned int dsdt_size);
int acpi_patch_qemu_tables(unsigned int base, unsigned int size);

#endif
