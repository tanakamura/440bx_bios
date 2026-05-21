#ifndef LEGACY_RM_H
#define LEGACY_RM_H

struct rm_int13_frame {
    unsigned short ax;
    unsigned short bx;
    unsigned short cx;
    unsigned short dx;
    unsigned short si;
    unsigned short di;
    unsigned short es;
    unsigned short ds;
    unsigned short bp;
    unsigned short ip;
    unsigned short cs;
    unsigned short flags;
    unsigned int eax32;
    unsigned int ebx32;
    unsigned int ecx32;
    unsigned int edx32;
    unsigned int esi32;
    unsigned int edi32;
    unsigned int ebp32;
    unsigned short fs;
    unsigned short gs;
} __attribute__((packed));

#endif
