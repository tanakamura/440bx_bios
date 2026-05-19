#ifndef BIOS_ACPI_RUNTIME_H
#define BIOS_ACPI_RUNTIME_H

void bios_acpi_install_for_linux(unsigned int rsdp_linear,
                                 unsigned int pm1_evt,
                                 unsigned int pm1_cnt,
                                 unsigned int gpe0,
                                 unsigned int gpe0_len,
                                 unsigned int flags);

#endif
