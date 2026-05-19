#include "bios_selftest.h"

#include "app/linux_loader/linux_loader.h"
#include "bios_serial.h"
#include "bios_storage.h"

static void cpu_serialize(void) {
    __asm__ volatile("xorl %%eax, %%eax\n\tcpuid"
                     :
                     :
                     : "eax", "ebx", "ecx", "edx", "memory");
}

void bios_selftest_run_elf_blob(const struct bios_selftest_config* config) {
    typedef unsigned int (*test_elf_entry_fn)(unsigned int, unsigned int,
                                             unsigned int, unsigned int);
    const struct bios_stage_context* stage = config->stage;
    blob_expand_fn expand = bios_stage_context_blob_expand(stage);
    void* blob_stage = bios_stage_context_blob_stage(stage);
    struct blob_status status;
    struct linux_loader_config linux_config = {0};
    unsigned char* image = (unsigned char*)LINUX_LOADER_TEST_ELF_IMAGE_LINEAR;
    unsigned int entry_phys = 0u;
    unsigned int rc;
    int expand_rc;

    if (stage->test_elf_blob_linear == 0u) {
        serial_write_string("No test ELF blob\r\n");
        return;
    }
    if (expand == 0 || blob_stage == 0) {
        serial_write_string("Test ELF blob service missing\r\n");
        return;
    }

    serial_write_string("Run ROM test ELF...\r\n");
    expand_rc = expand((const void*)stage->test_elf_blob_linear, blob_stage,
                       image, LINUX_LOADER_TEST_ELF_IMAGE_CAPACITY, &status,
                       stage->total_bytes);
    if (expand_rc != 0) {
        serial_write_string("Test ELF blob failed rc=");
        serial_write_hex8((unsigned char)expand_rc);
        serial_write_string(" block=");
        serial_write_hex32(status.block);
        serial_write_string("\r\n");
        return;
    }

    bios_linux_fill_loader_config(&linux_config, config->linux_config);
    if (linux_loader_load_elf_image(&linux_config, image, status.output_size,
                                    &entry_phys) != 0) {
        return;
    }

    storage_scan(stage->total_bytes);
    config->install_legacy_runtime();
    config->install_boot_drive();
    config->install_pm_stack_top();
    config->install_vgabios_shadow();
    linux_loader_prepare_boot_params(&linux_config, entry_phys, 0u, 0u);

    serial_write_string("Call test ELF entry=");
    serial_write_hex32(entry_phys);
    serial_write_string(" params=");
    serial_write_hex32(LINUX_LOADER_BOOT_PARAMS);
    serial_write_string("\r\n");
    cpu_serialize();
    rc = ((test_elf_entry_fn)entry_phys)(
        LINUX_LOADER_BOOT_PARAMS, LINUX_LOADER_RSDP_LINEAR,
        stage->acpi_pm1_evt, stage->acpi_pm1_cnt);
    cpu_serialize();
    serial_write_string("Test ELF returned ");
    serial_write_hex32(rc);
    serial_write_string("\r\n");
}
