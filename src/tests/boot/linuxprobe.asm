bits 32

global _start

section .text
_start:
    cld
    mov ebx, esi
    cmp dword [ebx + 0x202], 0x53726448
    jne fail
    cmp byte [ebx + 0x1e8], 0
    je fail
    cmp dword [ebx + 0x228], 0
    je fail
    cmp dword [ebx + 0x21c], 0
    je .no_initrd
    mov eax, [ebx + 0x218]
    cmp byte [eax], 'I'
    je .no_initrd
    cmp byte [eax], '0'
    jne fail
.no_initrd:
    mov esi, ok_msg
    call puts_serial
    mov al, 42
    jmp exit_qemu

fail:
    mov esi, fail_msg
    call puts_serial
    mov al, 43

exit_qemu:
    mov dx, 0x00f4
    out dx, al
.halt:
    hlt
    jmp .halt

puts_serial:
    lodsb
    test al, al
    jz .done
    mov bl, al
.wait:
    mov dx, 0x03fd
    in al, dx
    test al, 0x20
    jz .wait
    mov al, bl
    mov dx, 0x03f8
    out dx, al
    jmp puts_serial
.done:
    ret

ok_msg:
    db 'LINUXPROBE', 13, 10, 0

fail_msg:
    db 'LINUXPROBEFAIL', 13, 10, 0
