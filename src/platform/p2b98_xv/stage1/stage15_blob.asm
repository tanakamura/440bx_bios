bits 32

%include "platform/p2b98_xv/stage1/stage15_crc.inc"

global stage15_blob_start
global stage15_blob_end
global stage15_blob_crc32

section .stage15_blob progbits alloc noexec nowrite align=16
stage15_blob_start:
    incbin "platform/p2b98_xv/stage15/stage15.bin"
stage15_blob_end:
align 4
stage15_blob_crc32:
    dd STAGE15_BLOB_CRC32
