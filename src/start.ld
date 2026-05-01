OUTPUT_FORMAT(elf32-i386)
ENTRY(start)

SECTIONS
{
    . = 0x000C0000;

    .rom_anchor :
    {
        BYTE(0x90)
    }

    . = 0x000F0000;

    .start :
    {
        *(.start)
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

    .bss :
    {
        *(.bss)
        *(COMMON)
    }

    . = 0x000FFFF0;

    .reset :
    {
        *(.reset)
    }

    /DISCARD/ :
    {
        *(.comment)
        *(.eh_frame)
        *(.note*)
    }
}
