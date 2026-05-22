#include "app/legacy/legacy_boot.h"
#include "app/legacy/legacy_floppy.h"
#include "app/legacy/legacy_service.h"
#include "app/legacy/legacy_thunk.h"
#include "app/linux_loader/linux_loader.h"
#include "app_runtime.h"
#include "bios_storage.h"

#define LINUX_VGABIOS_BASE_MEM_KB 640u
#define LINUX_VGABIOS_EBDA_SEG 0x0000u

void linux_vgabios_runtime_init(const struct app_boot_context* config) {
    struct legacy_service_context context;
    unsigned int floppy_dpt_linear;

    __asm__ volatile("cli" : : : "memory");
    legacy_boot_init(&config->platform);
    legacy_floppy_probe();
    floppy_dpt_linear = legacy_install_bios_thunks(
        legacy_floppy_present(), bios_hdd_is_present() != 0u,
        LINUX_VGABIOS_BASE_MEM_KB, LINUX_VGABIOS_EBDA_SEG, 0, 0);
    legacy_install_pm_stack_top(app_pm_stack_top(config->platform.total_bytes));
    context.platform = config->platform;
    context.floppy_dpt_linear = floppy_dpt_linear;
    context.base_mem_kb = LINUX_VGABIOS_BASE_MEM_KB;
    legacy_service_init(&context);
    __asm__ volatile("sti" : : : "memory");
}
