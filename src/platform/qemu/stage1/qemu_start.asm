bits 16

%define CODE_SEL 0x08
%define DATA_SEL 0x10
%define ROM_HIGH_DELTA 0xFFF00000
%define BLOB_STATUS_SIZE 20
%define STAGE2_LOAD_LINEAR 0x00080000
%define STAGE2_LOAD_CAPACITY 0x00010000
%define STAGE2_ENTRY 0x00080000
%define QEMU_TOTAL_BYTES (32 * 1024 * 1024)

%include "shared_service/service_table.inc"

global qemu_start

extern __blob_service_start
extern __blob_service_end
extern blob_shadow_load_and_enter
extern qemu_install_shared_service_table

section .start progbits alloc exec nowrite align=16
qemu_start:
    cli
    cld
    mov ax, cs
    mov ds, ax

    mov dx, 0x03f9
    xor al, al
    out dx, al
    mov dx, 0x03fa
    mov al, 0x07
    out dx, al
    mov dx, 0x03fb
    mov al, 0x80
    out dx, al
    mov dx, 0x03f8
    mov al, 0x01
    out dx, al
    mov dx, 0x03f9
    xor al, al
    out dx, al
    mov dx, 0x03fb
    mov al, 0x03
    out dx, al

    lgdt [qemu_gdtr - qemu_start]
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    db 0x66, 0xea
    dd qemu_pm_entry
    dw CODE_SEL

bits 32
qemu_pm_entry:
    mov ax, DATA_SEL
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov eax, QEMU_TOTAL_BYTES
    and eax, 0xfffff000
    sub eax, SHARED_TABLE_BYTES
    mov ebx, __blob_service_end
    sub ebx, __blob_service_start
    add ebx, 15
    and ebx, 0xfffffff0
    sub eax, ebx
    mov ebp, eax
    mov esp, eax

    mov esi, __blob_service_start + ROM_HIGH_DELTA
    mov edi, ebp
    mov ecx, __blob_service_end
    sub ecx, __blob_service_start
    rep movsb
    xor eax, eax
    cpuid
    mov esi, esp

    push ebp
    push esi
    push dword QEMU_TOTAL_BYTES
    call qemu_install_shared_service_table
    add esp, 12

    sub esp, BLOB_STATUS_SIZE
    mov ebx, esp
    push dword STAGE2_ENTRY
    push dword QEMU_TOTAL_BYTES
    push ebx
    push dword STAGE2_LOAD_CAPACITY
    push dword STAGE2_LOAD_LINEAR
    push dword 0
    push dword 0
    mov eax, blob_shadow_load_and_enter
    sub eax, __blob_service_start
    add eax, ebp
    call eax
.hang:
    hlt
    jmp .hang

align 8
qemu_gdt_start:
    dq 0x0000000000000000
    dq 0x00cf9b000000ffff
    dq 0x00cf93000000ffff
    dq 0x00009b0fe000ffff
    dq 0x0000930fe000ffff
qemu_gdt_end:

qemu_gdtr:
    dw qemu_gdt_end - qemu_gdt_start - 1
    dd qemu_gdt_start

section .reset progbits alloc exec nowrite align=16
bits 16
qemu_reset_vector:
    jmp 0xF000:0x0000
