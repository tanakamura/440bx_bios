#include "app_loader.h"

#include "bios_serial.h"
#include "blob.h"

unsigned int app_load_payload(const struct bios_stage_context* stage,
                              unsigned int payload_id, const char* label) {
    blob_load_fn load = bios_stage_context_blob_load(stage);
    struct blob_status status;
    unsigned int load_addr = APP_SLOT_LOAD_LINEAR;
    int rc;

    if (load == 0) {
        return 0u;
    }

    rc = load(payload_id, (void*)APP_SLOT_LOAD_LINEAR,
              APP_SLOT_LOAD_CAPACITY, &load_addr, &status,
              stage->total_bytes);
    if (rc == 0) {
        serial_write_string(label);
        serial_write_string(" app @ ");
        serial_write_hex32(load_addr);
        serial_write_string("\r\n");
        return load_addr;
    }

    serial_write_string(label);
    serial_write_string(" app load failed rc=");
    serial_write_hex32((unsigned int)rc);
    serial_write_string("\r\n");
    return 0u;
}
