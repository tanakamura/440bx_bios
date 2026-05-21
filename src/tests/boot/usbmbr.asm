bits 16
org 0x7c00

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7c00
    mov bl, dl
    sti
    mov si, msg
    call puts_serial
    cmp bl, 0x80
    jne fail
    call test_timer
    jc fail
    mov si, timer_ok_msg
    call puts_serial
    call test_int60
    jc fail
    mov si, int60_ok_msg
    call puts_serial
    call test_e820
    jc fail
    mov si, e820_ok_msg
    call puts_serial
    mov al, 42
    jmp exit_qemu

fail:
    mov si, fail_msg
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
    call putc_serial
    jmp puts_serial
.done:
    ret

putc_serial:
    push ax
.wait:
    mov dx, 0x03fd
    in al, dx
    test al, 0x20
    jz .wait
    pop ax
    mov dx, 0x03f8
    out dx, al
    ret

test_int60:
    mov byte [int60_buf], 0x12
    xor cx, cx
    mov dx, int60_buf
    xor ax, ax
    int 0x60
    jc .fail
    cmp al, 0x12
    jne .fail

    xor cx, cx
    mov dx, int60_buf
    mov ax, 0x0134
    int 0x60
    jc .fail
    cmp byte [int60_buf], 0x34
    jne .fail
    clc
    ret
.fail:
    stc
    ret

test_timer:
    mov ax, [0x046c]
    mov dx, [0x046e]
    mov cx, 4
.wait:
    hlt
    cmp ax, [0x046c]
    jne .ok
    cmp dx, [0x046e]
    jne .ok
    loop .wait
    stc
    ret
.ok:
    clc
    ret

test_e820:
    xor ebx, ebx
    mov si, e820_expected
    mov bp, 4
.next:
    mov di, e820_buf
    call e820_call
    jc .fail
    cmp bx, [si + 0]
    jne .fail
    mov eax, [e820_buf + 0]
    cmp eax, [si + 2]
    jne .fail
    mov eax, [e820_buf + 8]
    cmp eax, [si + 6]
    jne .fail
    mov eax, [e820_buf + 16]
    cmp eax, [si + 10]
    jne .fail
    add si, 14
    dec bp
    jnz .next
    clc
    ret
.fail:
    stc
    ret

e820_call:
    mov eax, 0xe820
    mov edx, 0x534d4150
    mov ecx, 20
    int 0x15
    jc .done
    cmp eax, 0x534d4150
    jne .bad
    cmp ecx, 20
    jb .bad
    clc
    ret
.bad:
    stc
.done:
    ret

msg:
    db 'USBMBR', 13, 10, 0

int60_ok_msg:
    db 'INT60OK', 13, 10, 0

timer_ok_msg:
    db 'TIMEROK', 13, 10, 0

e820_ok_msg:
    db 'E820OK', 13, 10, 0

fail_msg:
    db 'USBMBRFAIL', 13, 10, 0

e820_expected:
    dw 1
    dd 0x00000000, 0x0009fc00, 1
    dw 2
    dd 0x0009fc00, 0x00060400, 2
    dw 3
    dd 0x00100000, 0x01e00000, 1
    dw 0
    dd 0x01f00000, 0x00100000, 2

align 4
e820_buf:
    times 20 db 0

int60_buf:
    db 0

times 446 - ($ - $$) db 0

; One active dummy FAT12 partition is enough for BIOS MBR detection.
db 0x80, 0x00, 0x02, 0x00
db 0x01, 0x00, 0x02, 0x00
dd 1
dd 1
times 16 * 3 db 0
dw 0xaa55
