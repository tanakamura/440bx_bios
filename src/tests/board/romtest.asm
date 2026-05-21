bits 16
org 0

%define SIO_INDEX 0x15C
%define SIO_LDN   0x07
%define SIO_ACTR  0x30
%define SIO_IOAH  0x60
%define SIO_IOAL  0x61
%define UART_LDN  0x03
%define COM1_BASE 0x03F8

%define UART_THR  0
%define UART_IER  1
%define UART_LCR  3
%define UART_LSR  5

times 196608 db 0x90

start:
    test eax, eax
    jz .normal_boot
    out 0x80, al
.bist_halt:
    jmp short .bist_halt

.normal_boot:
    mov ax, cs
    mov ds, ax

    mov al, 0xd0
    out 0x80, al

    jmp init_superio_uart1
fini_init_superio_uart1:

    jmp init_uart
fini_init_uart:

    mov al, 0xd1
    out 0x80, al

    mov si, hello_msg
.send_next:
    lodsb
    test al, al
    jz .halt
    mov bl, al
.wait_thre:
    mov dx, COM1_BASE + UART_LSR
    in al, dx
    test al, 0x20
    jz .wait_thre

    mov dx, COM1_BASE + UART_THR
    mov al, bl
    out dx, al
    jmp short .send_next

.halt:
    mov al, 0xf0
    out 0x80, al
    jmp short .halt

init_superio_uart1:
    mov dx, SIO_INDEX
    mov al, SIO_LDN
    out dx, al
    inc dx
    mov al, UART_LDN
    out dx, al

    dec dx
    mov al, SIO_IOAH
    out dx, al
    inc dx
    mov al, COM1_BASE >> 8
    out dx, al

    dec dx
    mov al, SIO_IOAL
    out dx, al
    inc dx
    mov al, COM1_BASE & 0xff
    out dx, al

    dec dx
    mov al, SIO_ACTR
    out dx, al
    inc dx
    mov al, 0x01
    out dx, al
    jmp fini_init_superio_uart1

init_uart:
    mov dx, COM1_BASE + UART_IER
    xor al, al
    out dx, al

    mov dx, COM1_BASE + UART_LCR
    mov al, 0x80
    out dx, al

    mov dx, COM1_BASE + UART_THR
    mov al, 0x01
    out dx, al

    mov dx, COM1_BASE + UART_IER
    xor al, al
    out dx, al

    mov dx, COM1_BASE + UART_LCR
    mov al, 0x03
    out dx, al
    jmp fini_init_uart

hello_msg:
    db "romtest serial", 0x0d, 0x0a, 0

times (65536 - 16) - ($ - start) db 0x90

reset_vector:
    jmp 0xF000:0x0000

times 16 - ($ - reset_vector) db 0x90
