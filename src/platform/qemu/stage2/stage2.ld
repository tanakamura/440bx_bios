OUTPUT_FORMAT(elf32-i386)
ENTRY(qemu_stage2_entry)
INCLUDE include/blob.ld

SECTIONS
{
    . = STAGE2_LOAD_LINEAR;

    .stage2_entry :
    {
        KEEP(*(.stage2.entry))
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

    __qemu_stage2_bss_start = .;
    .bss :
    {
        *(.bss)
        *(.bss.*)
        *(COMMON)
    }
    __qemu_stage2_bss_end = .;

    ASSERT(. <= STAGE2_LOAD_LINEAR + STAGE2_LOAD_CAPACITY, "qemu stage2 exceeds load window")

    /DISCARD/ :
    {
        *(.comment)
        *(.eh_frame)
        *(.note*)
    }
}
