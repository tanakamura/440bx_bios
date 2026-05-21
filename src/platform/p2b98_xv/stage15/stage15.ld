OUTPUT_FORMAT(elf32-i386)
ENTRY(stage15_entry)
INCLUDE include/blob.ld

SECTIONS
{
    . = STAGE15_LOAD_LINEAR;

    .stage15_entry :
    {
        KEEP(*(.stage15.entry))
    }

    .blobsvc :
    {
        __blob_service_start = .;
        KEEP(*(.blobsvc.entry))
        KEEP(*(.blobsvc))
        __blob_service_end = .;
    }

    .text :
    {
        *(.text)
        *(.text.*)
        *(.iplt)
    }

    .rodata :
    {
        *(.rodata)
        *(.rodata.*)
    }

    .data :
    {
        *(.data)
        *(.data.*)
    }

    __stage15_bss_start = .;
    .bss :
    {
        *(.bss)
        *(.bss.*)
        *(COMMON)
    }
    __stage15_bss_end = .;

    ASSERT(. <= STAGE15_LOAD_LINEAR + STAGE15_LOAD_CAPACITY,
           "stage1.5 exceeds load window")

    /DISCARD/ :
    {
        *(.comment)
        *(.eh_frame)
        *(.note*)
    }
}
