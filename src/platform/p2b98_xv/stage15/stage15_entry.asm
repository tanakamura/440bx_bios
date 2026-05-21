bits 32

%include "include/blob.inc"

%define CODE_SEL  0x08
%define DATA_SEL  0x10
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

global stage15_entry
extern stage15_main

section .stage15.entry progbits alloc exec nowrite align=16
stage15_entry:
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
    xor edx, edx
    wrmsr

    mov ecx, IA32_MTRR_PHYSBASE0
    mov eax, 0x00000006
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
    call stage15_main

.halt:
    hlt
    jmp .halt
