%define CODE_SEL 0x08
%define DATA_SEL 0x10
%define CODE16_SEL 0x18
%define DATA16_SEL 0x20

%define THUNK_LINEAR 0x0009fc00
%define THUNK_SEG (THUNK_LINEAR >> 4)
%define PM_STACK_TOP_OFF (bios16_pm_stack_top - bios16_thunk_start)
%define RM_STACK_SS_OFF (bios16_rm_stack_ss - bios16_thunk_start)
%define RM_STACK_SP_OFF (bios16_rm_stack_sp - bios16_thunk_start)
%define SERVICE_VECTOR_OFF (bios16_service_vector - bios16_thunk_start)
%define RM_FRAME_OFF (bios16_rm_frame - bios16_thunk_start)
%define RM_EAX_OFF (bios16_rm_eax - bios16_thunk_start)
%define RM_EBX_OFF (bios16_rm_ebx - bios16_thunk_start)
%define RM_ECX_OFF (bios16_rm_ecx - bios16_thunk_start)
%define RM_EDX_OFF (bios16_rm_edx - bios16_thunk_start)
%define RM_ESI_OFF (bios16_rm_esi - bios16_thunk_start)
%define RM_EDI_OFF (bios16_rm_edi - bios16_thunk_start)
%define RM_EBP_OFF (bios16_rm_ebp - bios16_thunk_start)
%define RM_FS_OFF (bios16_rm_fs - bios16_thunk_start)
%define RM_GS_OFF (bios16_rm_gs - bios16_thunk_start)

bits 32

extern bios_rm_service

global bios_boot_freedos_pm32
global bios16_thunk_start
global bios16_int10
global bios16_int11
global bios16_int12
global bios16_int13
global bios16_int15
global bios16_int16
global bios16_int17
global bios16_int19
global bios16_int1a
global bios16_int60
global bios16_default
global bios16_thunk_end
global bios16_pm_stack_top

section .text progbits alloc exec nowrite align=16
bios_boot_freedos_pm32:
    cli
    jmp dword CODE16_SEL:(pm16_boot_entry - bios16_thunk_start)

int_pm_entry:
    mov ax, DATA_SEL
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, dword [THUNK_LINEAR + PM_STACK_TOP_OFF]
    xor eax, eax
    mov ax, word [THUNK_LINEAR + SERVICE_VECTOR_OFF]
    push dword (THUNK_LINEAR + RM_FRAME_OFF)
    push eax
    call bios_rm_service
    add esp, 8
    jmp dword CODE16_SEL:(pm16_service_return - bios16_thunk_start)

section .thunk16 progbits alloc exec align=16
bits 16

bios16_thunk_start:

bios16_pm_stack_top:
    dd 0

bios16_rm_stack_ss:
    dw 0

bios16_rm_stack_sp:
    dw 0

bios16_service_vector:
    dw 0

bios16_rm_frame:
    times 12 dw 0

bios16_rm_eax:
    dd 0
bios16_rm_ebx:
    dd 0
bios16_rm_ecx:
    dd 0
bios16_rm_edx:
    dd 0
bios16_rm_esi:
    dd 0
bios16_rm_edi:
    dd 0
bios16_rm_ebp:
    dd 0
bios16_rm_fs:
    dw 0
bios16_rm_gs:
    dw 0

pm16_boot_entry:
    mov ax, DATA16_SEL
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov eax, cr0
    and eax, 0xfffffffe
    mov cr0, eax
    jmp THUNK_SEG:(rm_boot_entry - bios16_thunk_start)

pm16_service_return:
    mov ax, DATA16_SEL
    mov ss, ax
    mov eax, cr0
    and eax, 0xfffffffe
    mov cr0, eax
    jmp THUNK_SEG:(rm_service_return - bios16_thunk_start)

rm_enter_pm:
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    db 0x66, 0xea
    dd int_pm_entry
    dw CODE_SEL

rm_boot_entry:
    cli
    cld
    mov al, 0xff
    out 0x21, al
    out 0xa1, al
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x8000
    xor bp, bp
    xor bx, bx
    xor cx, cx
    xor dx, dx
    xor si, si
    xor di, di
    xor eax, eax
    mov ax, sp
    mov esp, eax
    jmp 0x0000:0x7c00

bios16_int10:
    mov word [cs:SERVICE_VECTOR_OFF], 0x0010
    jmp short bios16_common

bios16_int11:
    mov word [cs:SERVICE_VECTOR_OFF], 0x0011
    jmp short bios16_common

bios16_int12:
    mov word [cs:SERVICE_VECTOR_OFF], 0x0012
    jmp short bios16_common

bios16_int13:
    mov word [cs:SERVICE_VECTOR_OFF], 0x0013
    jmp short bios16_common

bios16_int15:
    mov word [cs:SERVICE_VECTOR_OFF], 0x0015
    jmp short bios16_common

bios16_int16:
    mov word [cs:SERVICE_VECTOR_OFF], 0x0016
    jmp short bios16_common

bios16_int17:
    mov word [cs:SERVICE_VECTOR_OFF], 0x0017
    jmp short bios16_common

bios16_int19:
    mov word [cs:SERVICE_VECTOR_OFF], 0x0019
    jmp short bios16_common

bios16_int1a:
    mov word [cs:SERVICE_VECTOR_OFF], 0x001a
    jmp short bios16_common

bios16_int60:
    mov word [cs:SERVICE_VECTOR_OFF], 0x0060
    jmp short bios16_common

bios16_default:
    mov word [cs:SERVICE_VECTOR_OFF], 0x00ff
    jmp short bios16_common

bios16_common:
    mov [cs:RM_STACK_SS_OFF], ss
    mov [cs:RM_STACK_SP_OFF], sp
    mov [cs:RM_EAX_OFF], eax
    mov [cs:RM_EBX_OFF], ebx
    mov [cs:RM_ECX_OFF], ecx
    mov [cs:RM_EDX_OFF], edx
    mov [cs:RM_ESI_OFF], esi
    mov [cs:RM_EDI_OFF], edi
    mov [cs:RM_EBP_OFF], ebp
    mov [cs:RM_FS_OFF], fs
    mov [cs:RM_GS_OFF], gs
    mov [cs:RM_FRAME_OFF + 0], ax
    mov [cs:RM_FRAME_OFF + 2], bx
    mov [cs:RM_FRAME_OFF + 4], cx
    mov [cs:RM_FRAME_OFF + 6], dx
    mov [cs:RM_FRAME_OFF + 8], si
    mov [cs:RM_FRAME_OFF + 10], di
    mov [cs:RM_FRAME_OFF + 12], es
    mov [cs:RM_FRAME_OFF + 14], ds
    mov [cs:RM_FRAME_OFF + 16], bp
    mov bp, sp
    mov ax, [ss:bp + 0]
    mov [cs:RM_FRAME_OFF + 18], ax
    mov ax, [ss:bp + 2]
    mov [cs:RM_FRAME_OFF + 20], ax
    mov ax, [ss:bp + 4]
    mov [cs:RM_FRAME_OFF + 22], ax
    jmp rm_enter_pm

rm_service_return:
    mov ax, [cs:RM_FS_OFF]
    mov fs, ax
    mov ax, [cs:RM_GS_OFF]
    mov gs, ax
    mov ax, [cs:RM_STACK_SS_OFF]
    mov ss, ax
    mov bp, [cs:RM_STACK_SP_OFF]
    mov sp, bp
    xor eax, eax
    mov ax, bp
    mov esp, eax
    mov ax, [cs:RM_FRAME_OFF + 18]
    mov [ss:bp + 0], ax
    mov ax, [cs:RM_FRAME_OFF + 20]
    mov [ss:bp + 2], ax
    mov ax, [cs:RM_FRAME_OFF + 22]
    mov [ss:bp + 4], ax
    mov ebx, [cs:RM_EBX_OFF]
    mov ecx, [cs:RM_ECX_OFF]
    mov edx, [cs:RM_EDX_OFF]
    mov esi, [cs:RM_ESI_OFF]
    mov edi, [cs:RM_EDI_OFF]
    mov ebp, [cs:RM_EBP_OFF]
    mov bx, [cs:RM_FRAME_OFF + 2]
    mov cx, [cs:RM_FRAME_OFF + 4]
    mov dx, [cs:RM_FRAME_OFF + 6]
    mov si, [cs:RM_FRAME_OFF + 8]
    mov di, [cs:RM_FRAME_OFF + 10]
    mov ax, [cs:RM_FRAME_OFF + 12]
    mov es, ax
    mov ax, [cs:RM_FRAME_OFF + 14]
    mov ds, ax
    mov bp, [cs:RM_FRAME_OFF + 16]
    mov eax, [cs:RM_EAX_OFF]
    mov ax, [cs:RM_FRAME_OFF + 0]
    iret

bios16_thunk_end:
