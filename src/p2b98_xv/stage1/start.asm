bits 16

%include "post_code.inc"

%define SIO_INDEX 0x15C
%define SIO_LDN   0x07
%define SIO_ACTR  0x30
%define SIO_IOAH  0x60
%define SIO_IOAL  0x61
%define UART_LDN  0x03
%define COM1_BASE 0x03F8

%define UART_THR  0
%define UART_IER  1
%define UART_FCR  2
%define UART_LCR  3
%define UART_MCR  4
%define CODE_SEL  0x08
%define DATA_SEL  0x10
%define CODE16_SEL 0x18
%define DATA16_SEL 0x20
%define START_DATA_SEG 0xFC00
%define CAR_STACK_TOP 0x000C1000
%define IA32_MTRR_PHYSBASE0   0x200
%define IA32_MTRR_PHYSMASK0   0x201
%define IA32_MTRR_FIX64K_00000 0x250
%define IA32_MTRR_FIX16K_80000 0x258
%define IA32_MTRR_FIX16K_A0000 0x259
%define IA32_MTRR_FIX4K_C0000 0x268
%define IA32_MTRR_FIX4K_C8000 0x269
%define IA32_MTRR_FIX4K_D0000 0x26A
%define IA32_MTRR_FIX4K_D8000 0x26B
%define IA32_MTRR_FIX4K_E0000 0x26C
%define IA32_MTRR_FIX4K_E8000 0x26D
%define IA32_MTRR_FIX4K_F0000 0x26E
%define IA32_MTRR_FIX4K_F8000 0x26F
%define IA32_MTRR_DEF_TYPE    0x2FF
%define MTRR_DEF_TYPE_E       0x00000800
%define MTRR_DEF_TYPE_FE      0x00000400
%define MTRR_PHYS_MASK_HIGH_36BIT 0x0000000F

extern c_entry
extern postcar_bootblock_resume

global start
global postcar_transition

section .start progbits alloc exec nowrite align=16
start:
    test eax, eax
    jz .normal_boot
    mov al, POST_BIST_FAIL
    out 0x80, al

.normal_boot:
    cli
    cld

    ; disable cache
    mov eax, cr0
    or eax, 0x40000000
    and eax, 0xDFFFFFFF
    mov cr0, eax
    mov ax, START_DATA_SEG
    mov ds, ax

    mov al, POST_BOOT
    out 0x80, al

    jmp init_superio_uart1
fini_init_superio_uart1:

    jmp init_uart
fini_init_uart:

    mov al, POST_UART_INIT_DONE
    out 0x80, al

    mov al, POST_BEFORE_CAR
    out 0x80, al

    jmp init_car
fini_init_car:
    mov al, POST_CAR_DONE
    out 0x80, al

    lgdt [gdtr - start_data]

    mov eax, cr0
    or eax, 0x00000001
    mov cr0, eax

    db 0x66, 0xea
    dd protected_mode_entry
    dw CODE_SEL

bits 32
protected_mode_entry:

    mov ax, DATA_SEL
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, CAR_STACK_TOP

    jmp c_entry

halt32:
    jmp short halt32

postcar_transition:
    cli

    mov ebx, [esp + 4]     ; new DRAM stack top
    mov edi, [esp + 8]     ; variable MTRR mask
    mov esi, [esp + 12]    ; total bytes
    mov ebp, [esp + 16]    ; SDRAM gdtr pointer

    mov eax, cr0
    or eax, 0x40000000
    and eax, 0xDFFFFFFF
    mov cr0, eax
    invd

    mov ecx, IA32_MTRR_DEF_TYPE
    xor eax, eax
    xor edx, edx
    wrmsr

    mov ecx, IA32_MTRR_FIX64K_00000
    mov eax, 0x06060606
    mov edx, 0x06060606
    wrmsr

    mov ecx, IA32_MTRR_FIX16K_80000
    mov eax, 0x06060606
    mov edx, 0x06060606
    wrmsr

    mov ecx, IA32_MTRR_FIX16K_A0000
    xor eax, eax
    xor edx, edx
    wrmsr

    mov ecx, IA32_MTRR_FIX4K_C0000
    xor eax, eax
    xor edx, edx
    wrmsr

    mov ecx, IA32_MTRR_FIX4K_C8000
    xor eax, eax
    xor edx, edx
    wrmsr

    mov ecx, IA32_MTRR_FIX4K_D0000
    xor eax, eax
    xor edx, edx
    wrmsr

    mov ecx, IA32_MTRR_FIX4K_D8000
    xor eax, eax
    xor edx, edx
    wrmsr

    mov ecx, IA32_MTRR_FIX4K_E0000
    xor eax, eax
    xor edx, edx
    wrmsr

    mov ecx, IA32_MTRR_FIX4K_E8000
    xor eax, eax
    xor edx, edx
    wrmsr

    mov ecx, IA32_MTRR_FIX4K_F0000
    xor eax, eax
    xor edx, edx
    wrmsr

    mov ecx, IA32_MTRR_FIX4K_F8000
    xor eax, eax
    ; 0x000fe000-0x000fffff contains the bootblock code.
    mov edx, 0x06060000
    wrmsr

    mov ecx, IA32_MTRR_PHYSBASE0
    mov eax, 0x00000006    ; WB @ 0
    xor edx, edx
    wrmsr

    mov ecx, IA32_MTRR_PHYSMASK0
    mov eax, edi
    mov edx, MTRR_PHYS_MASK_HIGH_36BIT
    wrmsr

    mov ecx, IA32_MTRR_DEF_TYPE
    mov eax, MTRR_DEF_TYPE_E | MTRR_DEF_TYPE_FE
    xor edx, edx
    wrmsr

    invd

    mov eax, cr0
    and eax, 0x9FFFFFFF
    mov cr0, eax

    lgdt [ebp]
    mov ax, DATA_SEL
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    mov esp, ebx
    push esi
    mov eax, postcar_bootblock_resume
    call eax
    jmp halt32

bits 16

init_car:
    mov eax, cr0
    or eax, 0x40000000
    and eax, 0xDFFFFFFF
    mov cr0, eax

    mov ecx, IA32_MTRR_DEF_TYPE
    xor eax, eax
    xor edx, edx
    wrmsr

    mov ecx, IA32_MTRR_FIX64K_00000
    xor eax, eax
    xor edx, edx
    wrmsr

    mov ecx, IA32_MTRR_FIX16K_80000
    xor eax, eax
    xor edx, edx
    wrmsr

    mov ecx, IA32_MTRR_FIX16K_A0000
    xor eax, eax
    xor edx, edx
    wrmsr

    mov ecx, IA32_MTRR_FIX4K_C0000
    mov eax, 0x00000006
    xor edx, edx
    wrmsr

    mov ecx, IA32_MTRR_FIX4K_C8000
    xor eax, eax
    xor edx, edx
    wrmsr

    mov ecx, IA32_MTRR_FIX4K_D0000
    xor eax, eax
    xor edx, edx
    wrmsr

    mov ecx, IA32_MTRR_FIX4K_D8000
    xor eax, eax
    xor edx, edx
    wrmsr

    mov ecx, IA32_MTRR_FIX4K_E0000
    xor eax, eax
    xor edx, edx
    wrmsr

    mov ecx, IA32_MTRR_FIX4K_E8000
    xor eax, eax
    xor edx, edx
    wrmsr

    mov ecx, IA32_MTRR_FIX4K_F0000
    xor eax, eax
    xor edx, edx
    wrmsr

    mov ecx, IA32_MTRR_FIX4K_F8000
    xor eax, eax
    ; 0x000fe000-0x000fffff contains the bootblock code.
    mov edx, 0x00000000
    wrmsr

    mov ecx, IA32_MTRR_DEF_TYPE
    mov eax, MTRR_DEF_TYPE_E | MTRR_DEF_TYPE_FE
    xor edx, edx
    wrmsr

    mov eax, cr0
    and eax, 0x9FFFFFFF
    mov cr0, eax

    ; fill 4k
    xor ax, ax
    mov es, ax
    mov eax, 0x9999aaaa
    mov edi, 0xc0000
    mov ecx, 4*1024/4   ; 4k
    rep stosd


    jmp fini_init_car

init_superio_uart1:
    mov dx, SIO_INDEX
    mov al, SIO_LDN
    out dx, al
    inc dx
    mov al, UART_LDN
    out dx, al

    dec dx
    mov al, SIO_IOAH
    out dx, al
    inc dx
    mov al, COM1_BASE >> 8
    out dx, al

    dec dx
    mov al, SIO_IOAL
    out dx, al
    inc dx
    mov al, COM1_BASE & 0xff
    out dx, al

    dec dx
    mov al, SIO_ACTR
    out dx, al
    inc dx
    mov al, 0x01
    out dx, al
    jmp fini_init_superio_uart1

init_uart:
    mov dx, COM1_BASE + UART_IER
    xor al, al
    out dx, al

    mov dx, COM1_BASE + UART_FCR
    mov al, 0x07
    out dx, al

    mov dx, COM1_BASE + UART_LCR
    mov al, 0x80
    out dx, al

    mov dx, COM1_BASE + UART_THR
    mov al, 0x01
    out dx, al

    mov dx, COM1_BASE + UART_IER
    xor al, al
    out dx, al

    mov dx, COM1_BASE + UART_LCR
    mov al, 0x03
    out dx, al

    mov dx, COM1_BASE + UART_MCR
    mov al, 0x03
    out dx, al

    jmp fini_init_uart

section .startdata progbits alloc noexec nowrite align=8
start_data:
align 8
gdt_start:
    dq 0x0000000000000000
    dq 0x00cf9b000000ffff
    dq 0x00cf93000000ffff
    dq 0x00009b0fe000ffff
    dq 0x0000930fe000ffff
gdt_end:

gdtr:
    dw gdt_end - gdt_start - 1
    dd gdt_start

section .reset progbits alloc exec nowrite align=16
bits 16
reset_vector:
    jmp 0xFE00:0x0000
