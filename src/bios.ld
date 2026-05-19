OUTPUT_FORMAT(elf32-i386)
ENTRY(postcar_resume)

SECTIONS
{
    . = 0x00200000;

    .entry :
    {
        *(.entry)
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

    .thunk16 :
    {
        *(.thunk16)
    }

    __bss_start = .;
    .bss :
    {
        *(.bss)
        *(COMMON)
    }
    __bss_end = .;

    ASSERT(. <= 0x00240000, "stage3 image exceeds high DRAM stage3 window")

    /DISCARD/ :
    {
        *(.comment)
        *(.eh_frame)
        *(.note*)
    }
}
