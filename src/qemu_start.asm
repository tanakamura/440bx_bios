bits 16

%define CODE_SEL 0x08
%define DATA_SEL 0x10
%define QEMU_BIOS_ENTRY 0x00101080
%define ROM_HIGH_DELTA 0xFFF00000
%define BLOB_SERVICE_LINEAR 0x00180000
%define BLOB_STAGE_LINEAR 0x00380000
%define BLOB_STATUS_SIZE 20
%define BIOS_LOAD_LINEAR 0x00100000
%define BIOS_LOAD_CAPACITY 0x00080000

global qemu_start

extern bios32_qemu_entry
extern __bios_blob_start
extern __fdos_blob_start
extern __blob_service_start
extern __blob_service_end

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
    mov esp, 0x001ff000

    mov esi, __blob_service_start + ROM_HIGH_DELTA
    mov edi, BLOB_SERVICE_LINEAR
    mov ecx, __blob_service_end
    sub ecx, __blob_service_start
    rep movsb
    xor eax, eax
    cpuid

    sub esp, BLOB_STATUS_SIZE
    mov ebx, esp
    push ebx
    push dword BIOS_LOAD_CAPACITY
    push dword BIOS_LOAD_LINEAR
    push dword BLOB_STAGE_LINEAR
    push dword __bios_blob_start + ROM_HIGH_DELTA
    call BLOB_SERVICE_LINEAR
    add esp, 20
    add esp, BLOB_STATUS_SIZE
    test eax, eax
    jnz .hang

    push dword __fdos_blob_start + ROM_HIGH_DELTA
    push dword (64 * 1024 * 1024)
    call QEMU_BIOS_ENTRY
.hang:
    hlt
    jmp .hang

align 8
qemu_gdt_start:
    dq 0x0000000000000000
    dq 0x00cf9b000000ffff
    dq 0x00cf93000000ffff
    dq 0x00009b100000ffff
    dq 0x000093100000ffff
qemu_gdt_end:

qemu_gdtr:
    dw qemu_gdt_end - qemu_gdt_start - 1
    dd qemu_gdt_start

section .reset progbits alloc exec nowrite align=16
bits 16
qemu_reset_vector:
    jmp 0xF000:0x0000
