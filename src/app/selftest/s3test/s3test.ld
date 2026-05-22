OUTPUT_FORMAT(elf32-i386)
ENTRY(app_entry)

SECTIONS
{
    . = 0x00180000;

    .text :
    {
        KEEP(*(.text.entry))
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
        *(.bss)
        *(.bss.*)
        *(COMMON)
    }

    /DISCARD/ :
    {
        *(.comment)
        *(.eh_frame)
        *(.note*)
    }
}
