#include "app_runtime.h"

#include "shared_service/service_table.h"

unsigned int app_pm_stack_top(unsigned int total_bytes) {
    struct shared_service_table* shared = shared_service_from_total(total_bytes);

    if (shared != 0 && shared->stack_top != 0u) {
        return shared->stack_top;
    }
    if (total_bytes >= 0x00300000u) {
        return (total_bytes & ~0xfffu) - 0x1000u;
    }
    return 0x001ff000u;
}
