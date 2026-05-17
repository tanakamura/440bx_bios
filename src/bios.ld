OUTPUT_FORMAT(elf32-i386)
ENTRY(postcar_resume)

SECTIONS
{
    . = 0x000F0000;

    .entry :
    {
        *(.entry)
    }

    . = 0x000F0080;

    .qentry :
    {
        *(.qentry)
    }

    . = ALIGN(16);

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

    __bss_start = .;
    .bss :
    {
        *(.bss)
        *(COMMON)
    }
    __bss_end = .;

    ASSERT(. <= 0x000FE000, "BIOS 32-bit body overlaps FE000 thunk area")

    . = 0x000FE000;

    .thunk16 :
    {
        *(.thunk16)
    }

    ASSERT(. <= 0x00100000, "BIOS image exceeds E0000-FFFFF")

    /DISCARD/ :
    {
        *(.comment)
        *(.eh_frame)
        *(.note*)
    }
}
