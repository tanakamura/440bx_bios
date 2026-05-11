#ifndef L2_SERVICE_H
#define L2_SERVICE_H

#define L2_SERVICE_LINEAR 0x00190000u

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

typedef int (*l2_enable_fn)(struct l2_status* status);

#endif
