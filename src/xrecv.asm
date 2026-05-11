org 0x100
bits 16

%define COM1_BASE 0x03f8
%define COM1_THR  (COM1_BASE + 0)
%define COM1_RBR  (COM1_BASE + 0)
%define COM1_LSR  (COM1_BASE + 5)

%define SOH 0x01
%define STX 0x02
%define EOT 0x04
%define ACK 0x06
%define NAK 0x15
%define CAN 0x18
%define CRCCHR 'C'

%define UART_TIMEOUT 0xffff

start:
    mov ax, cs
    mov ds, ax
    mov es, ax

    call parse_cmdline
    jc usage

    mov dx, filename_buf
    xor cx, cx
    mov ah, 0x3c
    int 0x21
    jc create_fail
    mov [file_handle], ax

    mov byte [expected_block], 1

start_wait:
    mov al, CRCCHR
    call uart_putc
    call uart_getc_timeout
    jc start_wait

    cmp al, EOT
    je recv_done_eot
    cmp al, CAN
    je recv_cancel
    cmp al, SOH
    je recv_128
    cmp al, STX
    je recv_1k
    jmp start_wait

recv_128:
    mov word [block_len], 128
    jmp recv_block

recv_1k:
    mov word [block_len], 1024

recv_block:
    call uart_getc_timeout
    jc recv_timeout
    mov bl, al
    call uart_getc_timeout
    jc recv_timeout
    mov bh, al

    mov al, bl
    not al
    cmp al, bh
    jne bad_block

    mov di, databuf
    mov cx, [block_len]
read_payload:
    jcxz payload_done
    call uart_getc_timeout
    jc recv_timeout
    stosb
    dec cx
    jmp read_payload
payload_done:

    call uart_getc_timeout
    jc recv_timeout
    mov ah, al
    call uart_getc_timeout
    jc recv_timeout
    mov dx, ax

    mov si, databuf
    mov cx, [block_len]
    call crc16_ccitt
    cmp ax, dx
    jne bad_block

    mov al, [expected_block]
    cmp bl, al
    je write_block
    dec al
    cmp bl, al
    je dup_block
    jmp bad_block

write_block:
    mov bx, [file_handle]
    mov ah, 0x40
    mov cx, [block_len]
    mov dx, databuf
    int 0x21
    jc io_fail
    mov al, [expected_block]
    inc al
    mov [expected_block], al

dup_block:
    mov al, ACK
    call uart_putc
    jmp wait_next

bad_block:
    mov al, NAK
    call uart_putc
    jmp wait_next

recv_timeout:
    mov al, NAK
    call uart_putc

wait_next:
    call uart_getc_timeout
    jc wait_next
    cmp al, EOT
    je recv_done_eot
    cmp al, CAN
    je recv_cancel
    cmp al, SOH
    je recv_128
    cmp al, STX
    je recv_1k
    jmp wait_next

recv_done_eot:
    mov al, ACK
    call uart_putc
    mov bx, [file_handle]
    mov ah, 0x3e
    int 0x21
    mov dx, msg_ok
    mov ah, 0x09
    int 0x21
    mov ax, 0x4c00
    int 0x21

recv_cancel:
    mov dx, msg_cancel
    mov ah, 0x09
    int 0x21
    mov ax, 0x4c05
    int 0x21

usage:
    mov dx, msg_usage
    mov ah, 0x09
    int 0x21
    mov ax, 0x4c01
    int 0x21

create_fail:
    mov dx, msg_create_fail
    mov ah, 0x09
    int 0x21
    mov ax, 0x4c02
    int 0x21

io_fail:
    mov dx, msg_io_fail
    mov ah, 0x09
    int 0x21
    mov ax, 0x4c03
    int 0x21

parse_cmdline:
    mov si, 0x81
    mov cl, [0x80]
    xor ch, ch
skip_spaces:
    jcxz parse_cmdline_fail
    lodsb
    dec cx
    cmp al, ' '
    je skip_spaces
    cmp al, 9
    je skip_spaces
    lea di, [filename_buf]
copy_name:
    cmp al, ' '
    je done_name
    cmp al, 9
    je done_name
    stosb
    jcxz done_name
    lodsb
    dec cx
    jmp copy_name
done_name:
    mov byte [di], 0
    clc
    ret
parse_cmdline_fail:
    stc
    ret

uart_getc_timeout:
    mov cx, UART_TIMEOUT
.poll:
    mov dx, COM1_LSR
    in al, dx
    test al, 0x01
    jnz .have
    loop .poll
    stc
    ret
.have:
    mov dx, COM1_RBR
    in al, dx
    clc
    ret

uart_putc:
    push ax
.wait:
    mov dx, COM1_LSR
    in al, dx
    test al, 0x20
    jz .wait
    pop ax
    mov dx, COM1_THR
    out dx, al
    ret

crc16_ccitt:
    xor ax, ax
.next_byte:
    jcxz .done
    xor bh, bh
    mov bl, [si]
    inc si
    xor ah, bl
    mov di, 8
.bit:
    shl ax, 1
    jnc .no_xor
    xor ax, 0x1021
.no_xor:
    dec di
    jnz .bit
    dec cx
    jmp .next_byte
.done:
    ret

msg_usage db 'usage: XRECV output.bin', 13, 10, '$'
msg_create_fail db 'create failed', 13, 10, '$'
msg_io_fail db 'io failed', 13, 10, '$'
msg_cancel db 'cancelled', 13, 10, '$'
msg_ok db 'ok', 13, 10, '$'

file_handle dw 0ffffh
block_len dw 0
expected_block db 0
filename_buf times 64 db 0
databuf times 1024 db 0
