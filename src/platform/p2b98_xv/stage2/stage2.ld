OUTPUT_FORMAT(elf32-i386)
ENTRY(stage2_entry)
INCLUDE include/blob.ld

SECTIONS
{
    . = STAGE2_LOAD_LINEAR;

    .stage2_entry :
    {
        KEEP(*(.stage2.entry))
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

    __stage2_bss_start = .;
    .bss :
    {
        *(.bss)
        *(.bss.*)
        *(COMMON)
    }
    __stage2_bss_end = .;

    ASSERT(. <= STAGE2_LOAD_LINEAR + STAGE2_LOAD_CAPACITY, "stage2 exceeds load window")

    /DISCARD/ :
    {
        *(.comment)
        *(.eh_frame)
        *(.note*)
    }
}
