bits 32

%define CODE_SEL 0x08
%define DATA_SEL 0x10

extern vm86_saved_pm_esp
extern vm86_launch_eax
extern vm86_launch_ebx
extern vm86_launch_ecx
extern vm86_launch_edx
extern vm86_launch_esi
extern vm86_launch_edi
extern vm86_launch_ebp
extern vm86_launch_frame
extern vm86_result
extern vm86_handle_trap_c

global vm86_enter_asm
global vm86_gp_handler_asm
global vm86_ud_handler_asm

section .text progbits alloc exec nowrite align=16

vm86_enter_asm:
    pushfd
    pushad
    mov [vm86_saved_pm_esp], esp
    mov eax, [vm86_launch_eax]
    mov ebx, [vm86_launch_ebx]
    mov ecx, [vm86_launch_ecx]
    mov edx, [vm86_launch_edx]
    mov esi, [vm86_launch_esi]
    mov edi, [vm86_launch_edi]
    mov ebp, [vm86_launch_ebp]
    cli
    mov esp, vm86_launch_frame
    iretd

vm86_resume_pm:
    mov ax, DATA_SEL
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, [vm86_saved_pm_esp]
    popad
    popfd
    mov eax, [vm86_result]
    ret

vm86_gp_handler_asm:
    push dword 13
    jmp vm86_trap_common

vm86_ud_handler_asm:
    push dword 0
    push dword 6

vm86_trap_common:
    cld
    push gs
    push fs
    push es
    push ds
    pushad
    mov ax, DATA_SEL
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    push esp
    call vm86_handle_trap_c
    add esp, 4
    test eax, eax
    jnz vm86_trap_exit
    popad
    pop ds
    pop es
    pop fs
    pop gs
    add esp, 4
    add esp, 4
    iretd

vm86_trap_exit:
    add esp, 32
    add esp, 16
    add esp, 4
    add esp, 4
    jmp vm86_resume_pm
