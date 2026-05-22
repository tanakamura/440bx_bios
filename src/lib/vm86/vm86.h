#ifndef VM86_H
#define VM86_H

#include "legacy_rm.h"

struct vm86_run_regs {
    unsigned int eax;
    unsigned int ebx;
    unsigned int ecx;
    unsigned int edx;
    unsigned int esi;
    unsigned int edi;
    unsigned int ebp;
    unsigned short eflags16;
    unsigned short es;
    unsigned short ds;
    unsigned short fs;
    unsigned short gs;
    unsigned short ss;
    unsigned short sp;
    unsigned short cs;
    unsigned short ip;
};

struct vm86_io_ops {
    unsigned int (*in)(unsigned short port, unsigned int size, void* opaque);
    void (*out)(unsigned short port, unsigned int size, unsigned int value,
                void* opaque);
};

struct vm86_int_ops {
    void (*bios_int)(unsigned char vector, struct rm_int13_frame* frame,
                     void* opaque);
};

struct vm86_run_spec {
    struct vm86_run_regs regs;
    struct vm86_io_ops io;
    struct vm86_int_ops ints;
    void* opaque;
    unsigned int thunk_linear_base;
};

int vm86_run(struct vm86_run_spec* spec);
int vm86_in_bios_int_hook(void);

#endif
