#ifndef L2_SERVICE_H
#define L2_SERVICE_H

struct l2_status {
    int rc;
    int l2r0;
    int l2r2;
    int l2r3;
    unsigned int before_ctl3;
    unsigned int after_ctl3;
    unsigned int total_kb;
    unsigned int per_way_bytes;
    int alias_rc;
};

int l2_enable_service(struct l2_status* status);

#endif
