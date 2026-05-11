bits 16
org 0x7c00

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7c00
    cld

    mov [boot_drive], dl

    mov si, msg_boot
    call puts
    mov al, [boot_drive]
    call print_hex8
    call crlf

    mov si, msg_08
    call puts
    mov dl, [boot_drive]
    mov ah, 0x08
    int 0x13
    call print_status_and_regs

    mov si, msg_15
    call puts
    mov dl, [boot_drive]
    mov ax, 0x1500
    int 0x13
    call print_status_and_regs

    mov si, msg_16
    call puts
    mov dl, [boot_drive]
    mov ax, 0x1600
    int 0x13
    call print_status_and_regs

    mov si, msg_r0
    call puts
    mov bx, 0x0500
    mov es, bx
    xor bx, bx
    mov dl, [boot_drive]
    mov ax, 0x0201
    mov cx, 0x0001
    mov dx, 0x0000
    int 0x13
    call print_status_and_regs
    jc .hang

    mov si, msg_d0
    call puts
    mov ax, 0x0500
    mov ds, ax
    xor si, si
    mov cx, 64
    call dump_bytes

    mov si, msg_r19
    xor ax, ax
    mov ds, ax
    call puts
    mov bx, 0x0600
    mov es, bx
    xor bx, bx
    mov dl, [boot_drive]
    mov ax, 0x0201
    mov cx, 0x0002
    mov dx, 0x0100
    int 0x13
    call print_status_and_regs
    jc .hang

    mov si, msg_d19
    xor ax, ax
    mov ds, ax
    call puts
    mov ax, 0x0600
    mov ds, ax
    xor si, si
    mov cx, 64
    call dump_bytes

.hang:
    hlt
    jmp .hang

print_status_and_regs:
    mov [saved_ax], ax
    mov [saved_bx], bx
    mov [saved_cx], cx
    mov [saved_dx], dx
    pushf
    pop ax
    mov [saved_flags], ax
    mov si, ax
    test si, 1
    jz .cf0
    mov al, 'C'
    call putc
    mov al, 'F'
    call putc
    mov al, '1'
    call putc
    jmp .cf_done
.cf0:
    mov al, 'C'
    call putc
    mov al, 'F'
    call putc
    mov al, '0'
    call putc
.cf_done:
    mov al, ' '
    call putc

    mov ax, [saved_ax]
    call print_ax
    mov al, ' '
    call putc
    mov ax, [saved_bx]
    call print_ax
    mov al, ' '
    call putc
    mov ax, [saved_cx]
    call print_ax
    mov al, ' '
    call putc
    mov ax, [saved_dx]
    call print_ax
    call crlf
    ret

dump_bytes:
    push ax
    push bx
    push dx
.loop:
    lodsb
    call print_hex8
    mov al, ' '
    call putc
    loop .loop
    call crlf
    pop dx
    pop bx
    pop ax
    ret

putc:
    out 0xe9, al
    ret

puts:
    lodsb
    test al, al
    jz .done
    call putc
    jmp puts
.done:
    ret

print_ax:
    push ax
    mov al, ah
    call print_hex8
    pop ax
    call print_hex8
    ret

print_hex8:
    push ax
    shr al, 4
    call print_hex4
    pop ax
    call print_hex4
    ret

print_hex4:
    and al, 0x0f
    cmp al, 10
    jb .digit
    add al, 'a' - 10
    jmp putc
.digit:
    add al, '0'
    jmp putc

crlf:
    mov al, 0x0d
    call putc
    mov al, 0x0a
    jmp putc

msg_boot db 'bootdl=', 0
msg_08 db '08 ', 0
msg_15 db '15 ', 0
msg_16 db '16 ', 0
msg_r0 db 'r00 ', 0
msg_d0 db 'd00 ', 0
msg_r19 db 'r19 ', 0
msg_d19 db 'd19 ', 0
boot_drive db 0
saved_ax dw 0
saved_bx dw 0
saved_cx dw 0
saved_dx dw 0
saved_flags dw 0

times 510 - ($ - $$) db 0
dw 0xaa55
