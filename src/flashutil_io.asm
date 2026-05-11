cpu 586
bits 16

%define CODE16_SEL 0x08
%define DATAFLAT_SEL 0x10

%define OP_READ8   1
%define OP_READ16  2
%define OP_READ32  3
%define OP_WRITE8  4
%define OP_WRITE16 5
%define OP_WRITE32 6

section .text

global _port_inpd_words
global _port_outpd_words
global port_inpd_words_
global port_outpd_words_

global _flat_read8
global _flat_read16
global _flat_read32_words
global _flat_write8
global _flat_write16
global _flat_write32_words
global flat_read8_
global flat_read16_
global flat_read32_words_
global flat_write8_
global flat_write16_
global flat_write32_words_

port_inpd_words_ equ _port_inpd_words
port_outpd_words_ equ _port_outpd_words
flat_read8_ equ _flat_read8
flat_read16_ equ _flat_read16
flat_read32_words_ equ _flat_read32_words
flat_write8_ equ _flat_write8
flat_write16_ equ _flat_write16
flat_write32_words_ equ _flat_write32_words

%macro DBGCHR 1
    mov dx, 03fdh
%%wait:
    in al, dx
    test al, 20h
    jz %%wait
    mov dx, 03f8h
    mov al, %1
    out dx, al
%endmacro

req_op:          db 0
req_addr_lo:     dw 0
req_addr_hi:     dw 0
req_data_lo:     dw 0
req_data_hi:     dw 0
ret_data_lo:     dw 0
ret_data_hi:     dw 0
saved_flags:     dw 0
saved_cr0:       dd 0
saved_ds:        dw 0
saved_es:        dw 0
saved_cs:        dw 0
saved_ss:        dw 0
saved_sp:        dw 0
rm_return_ptr:
    dw unreal_resume
    dw 0
gdtr:
    dw gdt_end - gdt - 1
    dd gdt
gdt:
    dq 0
gdt_code16:
    dw 0ffffh
    dw 0
    db 0
    db 09ah
    db 000h
    db 0
gdt_dataflat:
    dw 0ffffh
    dw 0
    db 0
    db 092h
    db 0cfh
    db 0
gdt_end:

_port_inpd_words:
    push bp
    mov bp, sp
    mov dx, [bp+4]
    in eax, dx
    mov bx, [bp+6]
    mov [bx], ax
    mov bx, [bp+8]
    shr eax, 16
    mov [bx], ax
    xor ax, ax
    pop bp
    ret

_port_outpd_words:
    push bp
    mov bp, sp
    mov dx, [bp+4]
    mov ax, [bp+6]
    mov bx, [bp+8]
    shl ebx, 16
    movzx eax, ax
    or eax, ebx
    out dx, eax
    pop bp
    ret

setup_gdt:
    mov ax, cs
    mov [cs:saved_cs], ax
    mov [cs:rm_return_ptr+2], ax
    xor eax, eax
    mov ax, cs
    shl eax, 4
    mov edx, eax
    add edx, gdt
    mov [cs:gdtr+2], edx
    mov [cs:gdt_code16+2], ax
    shr eax, 16
    mov [cs:gdt_code16+4], al
    mov [cs:gdt_code16+7], ah
    ret

do_flat_access:
    push bx
    push si
    push di
    push ds
    push es

    pushf
    pop ax
    mov [cs:saved_flags], ax
    mov ax, ds
    mov [cs:saved_ds], ax
    mov ax, es
    mov [cs:saved_es], ax
    mov ax, ss
    mov [cs:saved_ss], ax
    mov [cs:saved_sp], sp
    call setup_gdt
    cli

    DBGCHR 'a'
    mov eax, cr0
    mov [cs:saved_cr0], eax
    lgdt [cs:gdtr]
    or eax, 1
    mov cr0, eax
    jmp CODE16_SEL:prot_enter

prot_enter:
    DBGCHR 'b'
    mov ax, DATAFLAT_SEL
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov eax, [cs:saved_cr0]
    and eax, 0fffffffeh
    mov cr0, eax
    DBGCHR 'c'
    jmp far [cs:rm_return_ptr]

unreal_resume:
    DBGCHR 'd'
    mov ax, [cs:saved_ss]
    mov ss, ax
    mov sp, [cs:saved_sp]
    xor ebx, ebx
    xor eax, eax
    mov bx, [cs:req_addr_lo]
    mov ax, [cs:req_addr_hi]
    shl eax, 16
    or ebx, eax

    mov cl, [cs:req_op]
    cmp cl, OP_READ8
    je .read8
    cmp cl, OP_READ16
    je .read16
    cmp cl, OP_READ32
    je .read32
    cmp cl, OP_WRITE8
    je .write8
    cmp cl, OP_WRITE16
    je .write16
    cmp cl, OP_WRITE32
    je .write32
    jmp .done

.read8:
    xor eax, eax
    mov al, [ebx]
    mov [cs:ret_data_lo], ax
    xor ax, ax
    mov [cs:ret_data_hi], ax
    jmp .done

.read16:
    mov ax, [ebx]
    mov [cs:ret_data_lo], ax
    xor ax, ax
    mov [cs:ret_data_hi], ax
    jmp .done

.read32:
    mov eax, [ebx]
    mov [cs:ret_data_lo], ax
    shr eax, 16
    mov [cs:ret_data_hi], ax
    jmp .done

.write8:
    mov al, [cs:req_data_lo]
    mov [ebx], al
    jmp .done

.write16:
    mov ax, [cs:req_data_lo]
    mov [ebx], ax
    jmp .done

.write32:
    xor eax, eax
    mov ax, [cs:req_data_lo]
    xor edx, edx
    mov dx, [cs:req_data_hi]
    shl edx, 16
    or eax, edx
    mov [ebx], eax

.done:
    DBGCHR 'e'
    mov ax, [cs:saved_ds]
    mov ds, ax
    mov ax, [cs:saved_es]
    mov es, ax
    push word [cs:saved_flags]
    popf
    pop es
    pop ds
    pop di
    pop si
    pop bx
    ret

_flat_read8:
    push bp
    mov bp, sp
    mov byte [cs:req_op], OP_READ8
    mov ax, [bp+4]
    mov [cs:req_addr_lo], ax
    mov ax, [bp+6]
    mov [cs:req_addr_hi], ax
    call do_flat_access
    xor ax, ax
    mov al, [cs:ret_data_lo]
    pop bp
    ret

_flat_read16:
    push bp
    mov bp, sp
    mov byte [cs:req_op], OP_READ16
    mov ax, [bp+4]
    mov [cs:req_addr_lo], ax
    mov ax, [bp+6]
    mov [cs:req_addr_hi], ax
    call do_flat_access
    mov ax, [cs:ret_data_lo]
    pop bp
    ret

_flat_read32_words:
    push bp
    mov bp, sp
    mov byte [cs:req_op], OP_READ32
    mov ax, [bp+4]
    mov [cs:req_addr_lo], ax
    mov ax, [bp+6]
    mov [cs:req_addr_hi], ax
    call do_flat_access
    mov bx, [bp+8]
    mov ax, [cs:ret_data_lo]
    mov [bx], ax
    mov bx, [bp+10]
    mov ax, [cs:ret_data_hi]
    mov [bx], ax
    pop bp
    ret

_flat_write8:
    push bp
    mov bp, sp
    mov byte [cs:req_op], OP_WRITE8
    mov ax, [bp+4]
    mov [cs:req_addr_lo], ax
    mov ax, [bp+6]
    mov [cs:req_addr_hi], ax
    mov ax, [bp+8]
    mov [cs:req_data_lo], ax
    call do_flat_access
    pop bp
    ret

_flat_write16:
    push bp
    mov bp, sp
    mov byte [cs:req_op], OP_WRITE16
    mov ax, [bp+4]
    mov [cs:req_addr_lo], ax
    mov ax, [bp+6]
    mov [cs:req_addr_hi], ax
    mov ax, [bp+8]
    mov [cs:req_data_lo], ax
    call do_flat_access
    pop bp
    ret

_flat_write32_words:
    push bp
    mov bp, sp
    mov byte [cs:req_op], OP_WRITE32
    mov ax, [bp+4]
    mov [cs:req_addr_lo], ax
    mov ax, [bp+6]
    mov [cs:req_addr_hi], ax
    mov ax, [bp+8]
    mov [cs:req_data_lo], ax
    mov ax, [bp+10]
    mov [cs:req_data_hi], ax
    call do_flat_access
    pop bp
    ret
