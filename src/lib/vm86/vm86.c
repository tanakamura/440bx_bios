#include "vm86.h"

#include "bios_io.h"
#include "bios_serial.h"

#define VM86_FLAG_CF 0x0001u
#define VM86_FLAG_IF 0x0200u
#define VM86_FLAG_VM 0x00020000u
#define VM86_FLAG_IOPL3 0x00003000u

#define VM86_SENTINEL_LINEAR 0x00007000u
#define VM86_SENTINEL_SEG (VM86_SENTINEL_LINEAR >> 4)
#define VM86_SENTINEL_OFF 0x0000u

#define VM86_GDT_SEL_CODE 0x08u
#define VM86_GDT_SEL_DATA 0x10u
#define VM86_GDT_SEL_CODE16 0x18u
#define VM86_GDT_SEL_DATA16 0x20u
#define VM86_GDT_SEL_TSS 0x28u

struct vm86_tss32 {
    unsigned short prev_task;
    unsigned short reserved0;
    unsigned int esp0;
    unsigned short ss0;
    unsigned short reserved1;
    unsigned int esp1;
    unsigned short ss1;
    unsigned short reserved2;
    unsigned int esp2;
    unsigned short ss2;
    unsigned short reserved3;
    unsigned int cr3;
    unsigned int eip;
    unsigned int eflags;
    unsigned int eax;
    unsigned int ecx;
    unsigned int edx;
    unsigned int ebx;
    unsigned int esp;
    unsigned int ebp;
    unsigned int esi;
    unsigned int edi;
    unsigned short es;
    unsigned short reserved4;
    unsigned short cs;
    unsigned short reserved5;
    unsigned short ss;
    unsigned short reserved6;
    unsigned short ds;
    unsigned short reserved7;
    unsigned short fs;
    unsigned short reserved8;
    unsigned short gs;
    unsigned short reserved9;
    unsigned short ldt;
    unsigned short reserved10;
    unsigned short trap;
    unsigned short iomap_base;
} __attribute__((packed));

struct vm86_idt_gate32 {
    unsigned short offset_lo;
    unsigned short selector;
    unsigned char zero;
    unsigned char type_attr;
    unsigned short offset_hi;
} __attribute__((packed));

struct vm86_desc_ptr32 {
    unsigned short limit;
    unsigned int base;
} __attribute__((packed));

struct vm86_gpr_frame {
    unsigned int edi;
    unsigned int esi;
    unsigned int ebp;
    unsigned int esp_dummy;
    unsigned int ebx;
    unsigned int edx;
    unsigned int ecx;
    unsigned int eax;
    unsigned int ds_pm;
    unsigned int es_pm;
    unsigned int fs_pm;
    unsigned int gs_pm;
    unsigned int vector;
    unsigned int error;
    unsigned int eip;
    unsigned int cs;
    unsigned int eflags;
    unsigned int esp;
    unsigned int ss;
    unsigned int es;
    unsigned int ds;
    unsigned int fs;
    unsigned int gs;
};

extern int vm86_enter_asm(void);
extern void vm86_gp_handler_asm(void);
extern void vm86_ud_handler_asm(void);

static const unsigned long long vm86_gdt_template[] = {
    0x0000000000000000ull, 0x00cf9b000000ffffull, 0x00cf93000000ffffull,
    0x00009b0fe000ffffull, 0x0000930fe000ffffull, 0x0000000000000000ull,
};

static unsigned long long vm86_gdt[6] __attribute__((aligned(16)));
static struct vm86_idt_gate32 vm86_idt[256] __attribute__((aligned(16)));
static struct vm86_tss32 vm86_tss __attribute__((aligned(16)));
static unsigned char vm86_pm_stack[4096] __attribute__((aligned(16)));
static struct vm86_run_spec* vm86_active_spec;
static unsigned char vm86_in_hook;
unsigned int vm86_saved_pm_esp;
unsigned int vm86_launch_eax;
unsigned int vm86_launch_ebx;
unsigned int vm86_launch_ecx;
unsigned int vm86_launch_edx;
unsigned int vm86_launch_esi;
unsigned int vm86_launch_edi;
unsigned int vm86_launch_ebp;
unsigned int vm86_launch_frame[9];
int vm86_result;

static void vm86_set_desc_tss(unsigned long long* desc, unsigned int base,
                              unsigned int limit) {
    unsigned long long value = 0;
    value |= (unsigned long long)(limit & 0xffffu);
    value |= (unsigned long long)(base & 0xffffu) << 16;
    value |= (unsigned long long)((base >> 16) & 0xffu) << 32;
    value |= (unsigned long long)0x89u << 40;
    value |= (unsigned long long)((limit >> 16) & 0x0fu) << 48;
    value |= (unsigned long long)((base >> 24) & 0xffu) << 56;
    *desc = value;
}

static void vm86_set_idt_gate(struct vm86_idt_gate32* gate, void (*fn)(void)) {
    unsigned int addr = (unsigned int)fn;
    gate->offset_lo = (unsigned short)addr;
    gate->selector = VM86_GDT_SEL_CODE;
    gate->zero = 0;
    gate->type_attr = 0x8eu;
    gate->offset_hi = (unsigned short)(addr >> 16);
}

static void vm86_install_tables(void) {
    struct vm86_desc_ptr32 gdtr;
    struct vm86_desc_ptr32 idtr;
    unsigned int i;

    for (i = 0; i < 5u; ++i) {
        vm86_gdt[i] = vm86_gdt_template[i];
    }
    vm86_set_desc_tss(&vm86_gdt[5], (unsigned int)&vm86_tss,
                      (unsigned int)(sizeof(vm86_tss) - 1u));
    for (i = 0; i < 256u; ++i) {
        vm86_set_idt_gate(&vm86_idt[i], vm86_ud_handler_asm);
    }
    vm86_set_idt_gate(&vm86_idt[13], vm86_gp_handler_asm);
    vm86_set_idt_gate(&vm86_idt[6], vm86_ud_handler_asm);

    vm86_tss = (struct vm86_tss32){0};
    vm86_tss.ss0 = VM86_GDT_SEL_DATA;
    vm86_tss.esp0 = (unsigned int)(vm86_pm_stack + sizeof(vm86_pm_stack) - 16u);
    vm86_tss.iomap_base = (unsigned short)sizeof(vm86_tss);

    gdtr.limit = (unsigned short)(sizeof(vm86_gdt) - 1u);
    gdtr.base = (unsigned int)vm86_gdt;
    idtr.limit = (unsigned short)(sizeof(vm86_idt) - 1u);
    idtr.base = (unsigned int)vm86_idt;

    __asm__ volatile("lgdt %0" : : "m"(gdtr) : "memory");
    __asm__ volatile(
        "movw %0, %%ax\n\t"
        "movw %%ax, %%ds\n\t"
        "movw %%ax, %%es\n\t"
        "movw %%ax, %%fs\n\t"
        "movw %%ax, %%gs\n\t"
        "movw %%ax, %%ss\n\t"
        :
        : "i"(VM86_GDT_SEL_DATA)
        : "eax", "memory");
    __asm__ volatile("ltr %0" : : "r"((unsigned short)VM86_GDT_SEL_TSS)
                     : "memory");
    __asm__ volatile("lidt %0" : : "m"(idtr) : "memory");
}

static unsigned int vm86_linear(unsigned int seg, unsigned int off) {
    return ((seg & 0xffffu) << 4) + (off & 0xffffu);
}

static unsigned short vm86_stack_pop16(struct vm86_gpr_frame* frame) {
    unsigned int sp = frame->esp & 0xffffu;
    unsigned short value =
        *(volatile unsigned short*)vm86_linear(frame->ss, sp);
    sp = (sp + 2u) & 0xffffu;
    frame->esp = (frame->esp & 0xffff0000u) | sp;
    return value;
}

static void vm86_stack_push16(struct vm86_gpr_frame* frame,
                              unsigned short value) {
    unsigned int sp = (frame->esp - 2u) & 0xffffu;
    *(volatile unsigned short*)vm86_linear(frame->ss, sp) = value;
    frame->esp = (frame->esp & 0xffff0000u) | sp;
}

static unsigned short vm86_flags16(const struct vm86_gpr_frame* frame) {
    return (unsigned short)(frame->eflags & 0xffffu);
}

static void vm86_set_flags16(struct vm86_gpr_frame* frame,
                             unsigned short flags16) {
    frame->eflags = VM86_FLAG_VM | (unsigned int)flags16;
}

static void vm86_advance_ip(struct vm86_gpr_frame* frame, unsigned int bytes) {
    frame->eip = (frame->eip + bytes) & 0xffffu;
}

static void vm86_fill_rm_frame(const struct vm86_gpr_frame* s,
                               struct rm_int13_frame* f) {
    *f = (struct rm_int13_frame){
        .ax = (unsigned short)s->eax,
        .bx = (unsigned short)s->ebx,
        .cx = (unsigned short)s->ecx,
        .dx = (unsigned short)s->edx,
        .si = (unsigned short)s->esi,
        .di = (unsigned short)s->edi,
        .es = (unsigned short)s->es,
        .ds = (unsigned short)s->ds,
        .bp = (unsigned short)s->ebp,
        .ip = (unsigned short)s->eip,
        .cs = (unsigned short)s->cs,
        .flags = vm86_flags16(s),
        .eax32 = s->eax,
        .ebx32 = s->ebx,
        .ecx32 = s->ecx,
        .edx32 = s->edx,
        .esi32 = s->esi,
        .edi32 = s->edi,
        .ebp32 = s->ebp,
        .fs = (unsigned short)s->fs,
        .gs = (unsigned short)s->gs,
    };
}

static void vm86_store_rm_frame(struct vm86_gpr_frame* s,
                                const struct rm_int13_frame* f) {
    s->eax = (f->eax32 & 0xffff0000u) | f->ax;
    s->ebx = (f->ebx32 & 0xffff0000u) | f->bx;
    s->ecx = (f->ecx32 & 0xffff0000u) | f->cx;
    s->edx = (f->edx32 & 0xffff0000u) | f->dx;
    s->esi = (f->esi32 & 0xffff0000u) | f->si;
    s->edi = (f->edi32 & 0xffff0000u) | f->di;
    s->ebp = (f->ebp32 & 0xffff0000u) | f->bp;
    s->es = f->es;
    s->ds = f->ds;
    s->fs = f->fs;
    s->gs = f->gs;
    vm86_set_flags16(s, f->flags);
}

static int vm86_handle_host_int(struct vm86_gpr_frame* s,
                                unsigned char vector) {
    struct rm_int13_frame f;

    if (vm86_active_spec->ints.bios_int == 0) {
        return -1;
    }
    vm86_fill_rm_frame(s, &f);
    vm86_in_hook = 1u;
    vm86_active_spec->ints.bios_int(vector, &f, vm86_active_spec->opaque);
    vm86_in_hook = 0u;
    vm86_store_rm_frame(s, &f);
    vm86_advance_ip(s, 2u);
    return 0;
}

static int vm86_handle_internal_int(struct vm86_gpr_frame* s,
                                    unsigned char vector) {
    unsigned short off =
        *(volatile unsigned short*)((unsigned int)vector * 4u + 0u);
    unsigned short seg =
        *(volatile unsigned short*)((unsigned int)vector * 4u + 2u);
    unsigned int linear = vm86_linear(seg, off);
    unsigned short flags16 = vm86_flags16(s);

    serial_write_string("vm86 int=");
    serial_write_hex8(vector);
    serial_write_string(" ax=");
    serial_write_hex16((unsigned short)s->eax);
    serial_write_string(" bx=");
    serial_write_hex16((unsigned short)s->ebx);
    serial_write_string(" cx=");
    serial_write_hex16((unsigned short)s->ecx);
    serial_write_string(" dx=");
    serial_write_hex16((unsigned short)s->edx);
    serial_write_string(" cs:ip=");
    serial_write_hex16((unsigned short)s->cs);
    serial_write_string(":");
    serial_write_hex16((unsigned short)s->eip);
    serial_write_string("\r\n");

    if (vector == 0xffu) {
        vm86_result = 0;
        return 1;
    }
    if (linear >= vm86_active_spec->thunk_linear_base &&
        linear < vm86_active_spec->thunk_linear_base + 0x10000u) {
        return vm86_handle_host_int(s, vector);
    }

    vm86_stack_push16(s, flags16);
    vm86_stack_push16(s, (unsigned short)s->cs);
    vm86_stack_push16(s, (unsigned short)((s->eip + 2u) & 0xffffu));
    flags16 &= (unsigned short)~VM86_FLAG_IF;
    vm86_set_flags16(s, flags16);
    s->cs = seg;
    s->eip = off;
    return 0;
}

static int vm86_handle_inout(struct vm86_gpr_frame* s, unsigned char opcode,
                             unsigned int op32) {
    unsigned short port;
    unsigned int value = 0;
    unsigned int size = 1u;
    unsigned int len = 1u;

    if (opcode >= 0xe4u && opcode <= 0xe7u) {
        port = *(volatile unsigned char*)vm86_linear(s->cs, s->eip + 1u);
        len = 2u;
    } else {
        port = (unsigned short)s->edx;
    }
    if (opcode == 0xe5u || opcode == 0xe7u || opcode == 0xedu ||
        opcode == 0xefu) {
        size = op32 != 0u ? 4u : 2u;
    }

    if (opcode == 0xe4u || opcode == 0xe5u || opcode == 0xecu ||
        opcode == 0xedu) {
        if (vm86_active_spec->io.in == 0) {
            return -1;
        }
        value =
            vm86_active_spec->io.in(port, size, vm86_active_spec->opaque);
        if (size == 1u) {
            s->eax = (s->eax & 0xffffff00u) | (value & 0xffu);
        } else if (size == 2u) {
            s->eax = (s->eax & 0xffff0000u) | (value & 0xffffu);
        } else {
            s->eax = value;
        }
    } else {
        if (vm86_active_spec->io.out == 0) {
            return -1;
        }
        value = size == 1u ? (s->eax & 0xffu)
                           : size == 2u ? (s->eax & 0xffffu) : s->eax;
        vm86_active_spec->io.out(port, size, value, vm86_active_spec->opaque);
    }
    vm86_advance_ip(s, len);
    return 0;
}

static int vm86_handle_trap_inner(struct vm86_gpr_frame* s) {
    unsigned int linear = vm86_linear(s->cs, s->eip);
    unsigned char opcode;
    unsigned int ip = s->eip;
    unsigned int op32 = 0u;
    unsigned short flags16;

    for (;;) {
        opcode = *(volatile unsigned char*)linear;
        if (opcode == 0x66u) {
            op32 ^= 1u;
            linear += 1u;
            ip = (ip + 1u) & 0xffffu;
            continue;
        }
        if (opcode == 0x26u || opcode == 0x2eu || opcode == 0x36u ||
            opcode == 0x3eu || opcode == 0xf2u || opcode == 0xf3u) {
            linear += 1u;
            ip = (ip + 1u) & 0xffffu;
            continue;
        }
        break;
    }
    s->eip = ip;

    switch (opcode) {
        case 0xfau:
            flags16 = (unsigned short)(vm86_flags16(s) & ~VM86_FLAG_IF);
            vm86_set_flags16(s, flags16);
            vm86_advance_ip(s, 1u);
            return 0;
        case 0xfbu:
            flags16 = (unsigned short)(vm86_flags16(s) | VM86_FLAG_IF);
            vm86_set_flags16(s, flags16);
            vm86_advance_ip(s, 1u);
            return 0;
        case 0x9cu:
            vm86_stack_push16(s, vm86_flags16(s));
            vm86_advance_ip(s, 1u);
            return 0;
        case 0x9du:
            vm86_set_flags16(s, vm86_stack_pop16(s));
            vm86_advance_ip(s, 1u);
            return 0;
        case 0xcfu:
            s->eip = vm86_stack_pop16(s);
            s->cs = vm86_stack_pop16(s);
            vm86_set_flags16(s, vm86_stack_pop16(s));
            return 0;
        case 0xcdu:
            return vm86_handle_internal_int(
                s, *(volatile unsigned char*)vm86_linear(s->cs, s->eip + 1u));
        case 0xe4u:
        case 0xe5u:
        case 0xe6u:
        case 0xe7u:
        case 0xecu:
        case 0xedu:
        case 0xeeu:
        case 0xefu:
            return vm86_handle_inout(s, opcode, op32);
        case 0xf4u:
            vm86_result = 0;
            return 1;
        default:
            serial_write_string("vm86 unsupported op=");
            serial_write_hex8(opcode);
            serial_write_string(" cs:ip=");
            serial_write_hex16((unsigned short)s->cs);
            serial_write_string(":");
            serial_write_hex16((unsigned short)s->eip);
            serial_write_string("\r\n");
            vm86_result = -1;
            return 1;
    }
}

int vm86_handle_trap_c(struct vm86_gpr_frame* state) {
    unsigned int vector = state->vector;

    if (vector != 13u && vector != 6u) {
        vm86_result = -1;
        return 1;
    }
    return vm86_handle_trap_inner(state);
}

static unsigned int vm86_direct_in(unsigned short port, unsigned int size,
                                   void* opaque) {
    (void)opaque;
    if (size == 1u) {
        return inb(port);
    }
    if (size == 2u) {
        return inw(port);
    }
    return inl(port);
}

static void vm86_direct_out(unsigned short port, unsigned int size,
                            unsigned int value, void* opaque) {
    (void)opaque;
    if (size == 1u) {
        outb(port, (unsigned char)value);
    } else if (size == 2u) {
        outw(port, (unsigned short)value);
    } else {
        outl(port, value);
    }
}

int vm86_run(struct vm86_run_spec* spec) {
    unsigned char pic1_mask;
    unsigned char pic2_mask;
    int rc;

    vm86_active_spec = spec;
    vm86_result = -1;
    if (spec == 0 || spec->ints.bios_int == 0) {
        return -1;
    }
    if (spec->io.in == 0) {
        spec->io.in = vm86_direct_in;
    }
    if (spec->io.out == 0) {
        spec->io.out = vm86_direct_out;
    }

    *(volatile unsigned char*)VM86_SENTINEL_LINEAR = 0xcdu;
    *(volatile unsigned char*)(VM86_SENTINEL_LINEAR + 1u) = 0xffu;

    vm86_install_tables();

    vm86_launch_eax = spec->regs.eax;
    vm86_launch_ebx = spec->regs.ebx;
    vm86_launch_ecx = spec->regs.ecx;
    vm86_launch_edx = spec->regs.edx;
    vm86_launch_esi = spec->regs.esi;
    vm86_launch_edi = spec->regs.edi;
    vm86_launch_ebp = spec->regs.ebp;
    vm86_launch_frame[0] = spec->regs.ip;
    vm86_launch_frame[1] = spec->regs.cs;
    vm86_launch_frame[2] = VM86_FLAG_VM | spec->regs.eflags16;
    vm86_launch_frame[3] = spec->regs.sp;
    vm86_launch_frame[4] = spec->regs.ss;
    vm86_launch_frame[5] = spec->regs.es;
    vm86_launch_frame[6] = spec->regs.ds;
    vm86_launch_frame[7] = spec->regs.fs;
    vm86_launch_frame[8] = spec->regs.gs;

    pic1_mask = inb(0x21u);
    pic2_mask = inb(0xa1u);
    outb(0x21u, 0xffu);
    outb(0xa1u, 0xffu);
    rc = vm86_enter_asm();
    outb(0x21u, pic1_mask);
    outb(0xa1u, pic2_mask);
    return rc;
}
int vm86_in_bios_int_hook(void) { return vm86_in_hook != 0u; }
