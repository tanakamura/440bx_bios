org 0x100
bits 16

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

main_loop:
    call read_line
    jc finish_ok
    cmp byte [linebuf], 0
    je main_loop
    cmp byte [linebuf], 'S'
    jne main_loop

    mov al, [linebuf + 1]
    cmp al, '1'
    je rec_s1
    cmp al, '2'
    je rec_s2
    cmp al, '3'
    je rec_s3
    cmp al, '7'
    je finish_ok
    cmp al, '8'
    je finish_ok
    cmp al, '9'
    je finish_ok
    jmp main_loop

rec_s1:
    mov byte [addr_len], 2
    jmp process_data
rec_s2:
    mov byte [addr_len], 3
    jmp process_data
rec_s3:
    mov byte [addr_len], 4

process_data:
    mov si, linebuf + 2
    call parse_hex_byte
    jc parse_fail
    mov [count_byte], al

    xor ax, ax
    xor dx, dx
    mov [addr_lo], ax
    mov [addr_hi], dx

    xor ch, ch
    mov cl, [addr_len]
addr_loop:
    jcxz addr_done
    push cx
    call parse_hex_byte
    pop cx
    jc parse_fail
    call shl_addr_8
    xor ah, ah
    add [addr_lo], ax
    adc word [addr_hi], 0
    dec cx
    jmp addr_loop
addr_done:
    mov al, [count_byte]
    xor ah, ah
    sub ax, [addr_len]
    sub ax, 1
    mov [data_len], ax
    jbe main_loop

    cmp byte [base_valid], 0
    jne base_ready
    mov ax, [addr_lo]
    mov [base_lo], ax
    mov ax, [addr_hi]
    mov [base_hi], ax
    mov byte [base_valid], 1
base_ready:
    mov ax, [addr_lo]
    mov dx, [addr_hi]
    sub ax, [base_lo]
    sbb dx, [base_hi]
    mov [offset_lo], ax
    mov [offset_hi], dx

    mov ax, [offset_lo]
    cmp ax, [curpos_lo]
    jne do_seek
    mov ax, [offset_hi]
    cmp ax, [curpos_hi]
    jne do_seek
    jmp seek_done

do_seek:
    mov bx, [file_handle]
    xor al, al
    mov ah, 0x42
    mov cx, [offset_hi]
    mov dx, [offset_lo]
    int 0x21
    jc io_fail
    mov [curpos_lo], ax
    mov [curpos_hi], dx

seek_done:
    mov di, databuf
    mov cx, [data_len]
decode_loop:
    jcxz decode_done
    push cx
    call parse_hex_byte
    pop cx
    jc parse_fail
    stosb
    dec cx
    jmp decode_loop
decode_done:
    call parse_hex_byte
    jc parse_fail

    mov bx, [file_handle]
    mov ah, 0x40
    mov cx, [data_len]
    mov dx, databuf
    int 0x21
    jc io_fail

    mov ax, [curpos_lo]
    mov dx, [curpos_hi]
    add ax, [data_len]
    adc dx, 0
    mov [curpos_lo], ax
    mov [curpos_hi], dx
    jmp main_loop

finish_ok:
    mov bx, [file_handle]
    cmp bx, 0xffff
    je print_ok
    mov ah, 0x3e
    int 0x21
print_ok:
    mov dx, msg_ok
    mov ah, 0x09
    int 0x21
    mov ax, 0x4c00
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

parse_fail:
    mov dx, msg_parse_fail
    mov ah, 0x09
    int 0x21
    mov ax, 0x4c03
    int 0x21

io_fail:
    mov dx, msg_io_fail
    mov ah, 0x09
    int 0x21
    mov ax, 0x4c04
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

read_line:
    mov di, linebuf
    xor bx, bx
read_char:
    mov ah, 0x3f
    mov cx, 1
    mov dx, tmpchar
    int 0x21
    jc read_line_err
    cmp ax, 0
    je read_eof
    mov al, [tmpchar]
    cmp al, 0x1a
    je read_eof
    cmp al, 0x0d
    je read_char
    cmp al, 0x0a
    je read_done
    stosb
    inc bx
    cmp bx, 255
    jb read_char
read_done:
    mov byte [di], 0
    clc
    ret
read_eof:
    cmp bx, 0
    jne read_done
read_line_err:
    stc
    ret

parse_hex_byte:
    call parse_hex_nibble
    jc parse_hex_byte_fail
    shl al, 1
    shl al, 1
    shl al, 1
    shl al, 1
    mov ah, al
    call parse_hex_nibble
    jc parse_hex_byte_fail
    or al, ah
    clc
    ret
parse_hex_byte_fail:
    stc
    ret

parse_hex_nibble:
    lodsb
    cmp al, '0'
    jb bad_hex
    cmp al, '9'
    jbe is_dec
    cmp al, 'A'
    jb lower_hex
    cmp al, 'F'
    jbe is_upper
lower_hex:
    cmp al, 'a'
    jb bad_hex
    cmp al, 'f'
    ja bad_hex
    sub al, 'a' - 10
    clc
    ret
is_upper:
    sub al, 'A' - 10
    clc
    ret
is_dec:
    sub al, '0'
    clc
    ret
bad_hex:
    stc
    ret

shl_addr_8:
    mov ax, [addr_lo]
    mov dx, [addr_hi]
    mov dh, dl
    mov dl, ah
    mov ah, al
    mov al, 0
    mov [addr_lo], ax
    mov [addr_hi], dx
    ret

msg_usage db 'usage: SRECSAVE output.bin', 13, 10, '$'
msg_create_fail db 'create failed', 13, 10, '$'
msg_parse_fail db 'parse failed', 13, 10, '$'
msg_io_fail db 'io failed', 13, 10, '$'
msg_ok db 'ok', 13, 10, '$'

file_handle dw 0ffffh
base_valid db 0
addr_len db 0
count_byte db 0
tmpchar db 0
data_len dw 0
addr_lo dw 0
addr_hi dw 0
base_lo dw 0
base_hi dw 0
offset_lo dw 0
offset_hi dw 0
curpos_lo dw 0
curpos_hi dw 0

filename_buf times 64 db 0
linebuf times 260 db 0
databuf times 260 db 0
