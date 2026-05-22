#include "app_loader.h"

#include "bios_serial.h"

unsigned int app_load_payload(blob_load_fn load, unsigned int total_bytes,
                              unsigned int payload_id, const char* label) {
    struct blob_status status;
    unsigned int load_addr = APP_SLOT_LOAD_LINEAR;
    int rc;

    if (load == 0) {
        return 0u;
    }

    rc = load(payload_id, (void*)APP_SLOT_LOAD_LINEAR,
              APP_SLOT_LOAD_CAPACITY, &load_addr, &status,
              total_bytes);
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
    serial_write_string(" code=");
    serial_write_hex32((unsigned int)status.code);
    serial_write_string(" block=");
    serial_write_hex32(status.block);
    serial_write_string(" exp=");
    serial_write_hex32(status.expected);
    serial_write_string(" got=");
    serial_write_hex32(status.got);
    serial_write_string(" out=");
    serial_write_hex32(status.output_size);
    serial_write_string("\r\n");
    return 0u;
}
