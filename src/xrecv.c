#include <conio.h>
#include <dos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COM1_BASE 0x03f8
#define COM1_THR (COM1_BASE + 0)
#define COM1_RBR (COM1_BASE + 0)
#define COM1_IER (COM1_BASE + 1)
#define COM1_FCR (COM1_BASE + 2)
#define COM1_LCR (COM1_BASE + 3)
#define COM1_MCR (COM1_BASE + 4)
#define COM1_LSR (COM1_BASE + 5)
#define MCR_DTR 0x01
#define MCR_RTS 0x02

#define SOH 0x01
#define STX 0x02
#define EOT 0x04
#define ACK 0x06
#define NAK 0x15
#define CAN 0x18
#define CRCCHR 'C'

#define UART_TIMEOUT 0xffffUL

static unsigned char expected_block = 1;
static unsigned char databuf[1024];

static void uart_init_115200(void) {
    outp(COM1_IER, 0x00);
    outp(COM1_FCR, 0x07);
    outp(COM1_LCR, 0x80);
    outp(COM1_THR, 0x01);
    outp(COM1_IER, 0x00);
    outp(COM1_LCR, 0x03);
    outp(COM1_MCR, MCR_DTR | MCR_RTS);
}

static void uart_flow_resume(void) { outp(COM1_MCR, MCR_DTR | MCR_RTS); }

static int uart_getc_timeout(unsigned char* out) {
    unsigned long i;
    for (i = 0; i < UART_TIMEOUT; ++i) {
        if (inp(COM1_LSR) & 0x01) {
            *out = (unsigned char)inp(COM1_RBR);
            return 0;
        }
    }
    return -1;
}

static void uart_putc(unsigned char ch) {
    while ((inp(COM1_LSR) & 0x20) == 0) {
    }
    outp(COM1_THR, ch);
}

static unsigned short crc16_ccitt(const unsigned char* buf,
                                  unsigned short len) {
    unsigned short crc = 0;
    unsigned short i;

    while (len-- != 0) {
        crc ^= (unsigned short)(*buf++) << 8;
        for (i = 0; i < 8; ++i) {
            if (crc & 0x8000) {
                crc = (unsigned short)((crc << 1) ^ 0x1021);
            } else {
                crc <<= 1;
            }
        }
    }

    return crc;
}

static int recv_bytes(unsigned char* buf, unsigned short len) {
    unsigned short i;
    for (i = 0; i < len; ++i) {
        if (uart_getc_timeout(&buf[i]) != 0) {
            return -1;
        }
    }
    return 0;
}

static int recv_block(FILE* fp, unsigned short block_len) {
    unsigned char blkno;
    unsigned char blkno_inv;
    unsigned char crc_hi;
    unsigned char crc_lo;
    unsigned short recv_crc;
    unsigned short calc_crc;

    if (uart_getc_timeout(&blkno) != 0) {
        return -1;
    }
    if (uart_getc_timeout(&blkno_inv) != 0) {
        return -1;
    }
    if ((unsigned char)(blkno ^ blkno_inv) != 0xff) {
        return -2;
    }
    if (recv_bytes(databuf, block_len) != 0) {
        return -1;
    }
    if (uart_getc_timeout(&crc_hi) != 0 || uart_getc_timeout(&crc_lo) != 0) {
        return -1;
    }

    recv_crc = (unsigned short)(((unsigned short)crc_hi << 8) | crc_lo);
    calc_crc = crc16_ccitt(databuf, block_len);
    if (recv_crc != calc_crc) {
        return -2;
    }

    if (blkno == expected_block) {
        if (fwrite(databuf, 1, block_len, fp) != block_len) {
            return -3;
        }
        ++expected_block;
    } else if (blkno != (unsigned char)(expected_block - 1)) {
        return -2;
    }

    return 0;
}

int main(int argc, char** argv) {
    FILE* fp;
    unsigned char ch;
    int rc;

    if (argc != 2) {
        fputs("Usage: XRECV OUT.BIN\r\n", stdout);
        return 1;
    }

    fp = fopen(argv[1], "wb");
    if (fp == NULL) {
        fputs("create failed\r\n", stdout);
        return 2;
    }

    expected_block = 1;
    uart_init_115200();
    uart_flow_resume();

    for (;;) {
        uart_putc(CRCCHR);
        if (uart_getc_timeout(&ch) == 0) {
            break;
        }
    }

    for (;;) {
        if (ch == EOT) {
            uart_putc(ACK);
            fclose(fp);
            fputs("OK\r\n", stdout);
            return 0;
        }
        if (ch == CAN) {
            fclose(fp);
            fputs("cancel\r\n", stdout);
            return 5;
        }

        if (ch == SOH) {
            rc = recv_block(fp, 128);
        } else if (ch == STX) {
            rc = recv_block(fp, 1024);
        } else {
            rc = -2;
        }

        if (rc == 0) {
            uart_putc(ACK);
        } else if (rc == -3) {
            fclose(fp);
            fputs("write failed\r\n", stdout);
            return 3;
        } else {
            uart_putc(NAK);
        }

        while (uart_getc_timeout(&ch) != 0) {
        }
    }
}
