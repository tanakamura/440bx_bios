#include "l2_service.h"

#define L2SVC __attribute__((noinline))

#define MSR_BBL_CR_D0 0x00000088u
#define MSR_BBL_CR_D1 0x00000089u
#define MSR_BBL_CR_D2 0x0000008au
#define MSR_BBL_CR_D3 0x0000008bu
#define MSR_BBL_CR_ADDR 0x00000116u
#define MSR_BBL_CR_CTL 0x00000119u
#define MSR_BBL_CR_TRIG 0x0000011au
#define MSR_BBL_CR_BUSY 0x0000011bu
#define MSR_BBL_CR_CTL3 0x0000011eu

#define BBLCR3_L2_CONFIGURED (1u << 0)
#define BBLCR3_L2_ENABLED (1u << 8)
#define BBLCR3_L2_NOT_PRESENT (1u << 23)
#define BBLCR3_L2_SIZE_MASK (0x1fu << 13)
#define BBLCR3_L2_RANGE_MASK (0x7u << 20)
#define BBLCR3_L2_SIZE_512K (0x02u << 13)

#define L2CMD_CR 0x02u
#define L2CMD_CW 0x03u
#define L2CMD_TRR 0x0eu
#define L2CMD_TWW 0x1cu
#define L2CMD_MESI_I 0u

static L2SVC unsigned long long rdmsr(unsigned int msr) {
    unsigned int lo;
    unsigned int hi;
    __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((unsigned long long)hi << 32) | lo;
}

static L2SVC void wrmsr64(unsigned int msr, unsigned int lo, unsigned int hi) {
    __asm__ volatile("wrmsr" : : "c"(msr), "a"(lo), "d"(hi));
}

static L2SVC unsigned int read_cr0(void) {
    unsigned int value;
    __asm__ volatile("mov %%cr0, %0" : "=r"(value));
    return value;
}

static L2SVC void write_cr0(unsigned int value) {
    __asm__ volatile("mov %0, %%cr0" : : "r"(value) : "memory");
}

static L2SVC void disable_cache(void) {
    unsigned int cr0 = read_cr0();
    cr0 |= 0x40000000u;
    cr0 &= 0xdfffffffu;
    write_cr0(cr0);
}

static L2SVC void enable_cache(void) {
    unsigned int cr0 = read_cr0();
    cr0 &= 0x9fffffffu;
    write_cr0(cr0);
}

static L2SVC void wbinvd(void) { __asm__ volatile("wbinvd" : : : "memory"); }

static L2SVC int signal_l2(unsigned int address, unsigned int data_high,
                           unsigned int data_low, unsigned int way,
                           unsigned int command) {
    unsigned int i;
    unsigned int ctl;

    wrmsr64(MSR_BBL_CR_ADDR, address, 0);
    wrmsr64(MSR_BBL_CR_D0, data_low, data_high);
    wrmsr64(MSR_BBL_CR_D1, data_low, data_high);
    wrmsr64(MSR_BBL_CR_D2, data_low, data_high);
    wrmsr64(MSR_BBL_CR_D3, data_low, data_high);

    ctl = (unsigned int)rdmsr(MSR_BBL_CR_CTL);
    ctl = (ctl & 0xfffffce0u) | command | (way << 8);
    wrmsr64(MSR_BBL_CR_CTL, ctl, 0);
    wrmsr64(MSR_BBL_CR_TRIG, 0, 0);

    for (i = 0; i < 0x100u; ++i) {
        if ((((unsigned int)rdmsr(MSR_BBL_CR_BUSY)) & 1u) == 0) {
            return 0;
        }
    }
    return -1;
}

static L2SVC int read_l2_reg(unsigned int index) {
    unsigned int value;
    if (signal_l2(index << 5, 0, 0, 0, L2CMD_CR) != 0) {
        return -1;
    }
    value = (unsigned int)rdmsr(MSR_BBL_CR_ADDR);
    return (int)(value >> 21);
}

static L2SVC int write_l2_reg(unsigned int index, unsigned int data) {
    int v1;
    int v2;
    unsigned int i;

    v1 = read_l2_reg(0);
    if (v1 < 0) {
        return -1;
    }
    v2 = read_l2_reg(2);
    if (v2 < 0) {
        return -1;
    }

    if ((v1 & 0x20) == 0) {
        v2 &= 0x3;
        v2++;
    } else {
        v2 &= 0x7;
    }

    for (i = 0; i < (unsigned int)v2; ++i) {
        unsigned int data1 = (data & 0xffu) << 21;
        unsigned int data2 = (i << 11) & 0x1800u;
        data1 |= data2;
        data2 = (data2 << 6) & 0x20000u;
        data1 |= data2;
        if (signal_l2((index << 5) | data1, 0, 0, 0, L2CMD_CW) != 0) {
            return -1;
        }
    }
    return 0;
}

static L2SVC int test_l2_address_alias(unsigned int address1,
                                       unsigned int address2,
                                       unsigned int data_high,
                                       unsigned int data_low) {
    unsigned int d;
    unsigned long long msr;

    if (signal_l2(address1, data_high, data_low, 0, L2CMD_TWW) != 0) {
        return -1;
    }
    if (signal_l2(address2, 0, 0, 0, L2CMD_TRR) != 0) {
        return -1;
    }

    for (d = MSR_BBL_CR_D0; d <= MSR_BBL_CR_D3; ++d) {
        msr = rdmsr(d);
        if ((unsigned int)msr != data_low ||
            (unsigned int)(msr >> 32) != data_high) {
            return (int)((unsigned int)msr & 0xffffu);
        }
    }
    return 0;
}

static L2SVC void l2_status_set(struct l2_status* status, int rc) {
    if (status != 0) {
        status->rc = rc;
        status->after_ctl3 = (unsigned int)rdmsr(MSR_BBL_CR_CTL3);
    }
}

L2SVC int l2_enable_service(struct l2_status* status) {
    unsigned int bbl;
    int l2r0;
    int l2r2;
    int l2r3;
    unsigned int size_bits;
    unsigned int range_bits;
    unsigned int banks;
    unsigned int per_way_bytes;
    unsigned int total_kb;
    unsigned int lines;
    unsigned int line;
    unsigned int way;
    int alias_rc;

    if (status != 0) {
        status->rc = 0;
        status->l2r0 = -1;
        status->l2r2 = -1;
        status->l2r3 = -1;
        status->before_ctl3 = (unsigned int)rdmsr(MSR_BBL_CR_CTL3);
        status->after_ctl3 = status->before_ctl3;
        status->total_kb = 0;
        status->per_way_bytes = 0;
        status->alias_rc = -1;
    }

    bbl = (unsigned int)rdmsr(MSR_BBL_CR_CTL3);
    if ((bbl & BBLCR3_L2_NOT_PRESENT) != 0) {
        l2_status_set(status, -1);
        return -1;
    }

    l2r0 = read_l2_reg(0);
    l2r2 = read_l2_reg(2);
    l2r3 = read_l2_reg(3);
    if (status != 0) {
        status->l2r0 = l2r0;
        status->l2r2 = l2r2;
        status->l2r3 = l2r3;
    }
    if (l2r0 < 0 || l2r2 < 0 || l2r3 < 0) {
        l2_status_set(status, -2);
        return -2;
    }

    size_bits = BBLCR3_L2_SIZE_512K;
    range_bits = ((unsigned int)l2r3 & 0x7u) << 20;
    bbl &= ~(BBLCR3_L2_SIZE_MASK | BBLCR3_L2_RANGE_MASK);
    bbl |= size_bits | range_bits;
    wrmsr64(MSR_BBL_CR_CTL3, bbl, 0);

    banks = ((unsigned int)rdmsr(MSR_BBL_CR_CTL3) >> 11) & 0x3u;
    if (banks == 0) {
        banks = 1;
    }
    per_way_bytes = size_bits << 3;
    total_kb = (per_way_bytes * banks * 4u) >> 10;
    if (status != 0) {
        status->total_kb = total_kb;
        status->per_way_bytes = per_way_bytes;
    }

    alias_rc =
        test_l2_address_alias(0, per_way_bytes, 0xaaaaaaaau, 0x5555aaaau);
    if (status != 0) {
        status->alias_rc = alias_rc;
    }

    enable_cache();

    lines = per_way_bytes / 32u;
    for (line = 0; line < lines; ++line) {
        unsigned int address = (lines - 1u - line) * 32u;
        for (way = 0; way < 4u; ++way) {
            if (signal_l2(address, 0, 0, way, L2CMD_TWW | L2CMD_MESI_I) != 0) {
                l2_status_set(status, -3);
                return -3;
            }
        }
    }

    disable_cache();
    wbinvd();

    bbl = (unsigned int)rdmsr(MSR_BBL_CR_CTL3);
    bbl |= BBLCR3_L2_CONFIGURED;
    wrmsr64(MSR_BBL_CR_CTL3, bbl, 0);

    wbinvd();
    if (write_l2_reg(5, 0) != 0) {
        l2_status_set(status, -4);
        return -4;
    }

    bbl = (unsigned int)rdmsr(MSR_BBL_CR_CTL3);
    bbl |= BBLCR3_L2_ENABLED;
    wrmsr64(MSR_BBL_CR_CTL3, bbl, 0);

    enable_cache();
    l2_status_set(status, 0);
    return 0;
}
