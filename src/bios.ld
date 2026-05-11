OUTPUT_FORMAT(elf32-i386)
ENTRY(postcar_resume)

SECTIONS
{
    . = 0x00100000;

    .thunk16 :
    {
        *(.thunk16)
    }

    . = 0x00101000;

    .entry :
    {
        *(.entry)
    }

    . = 0x00101080;

    .qentry :
    {
        *(.qentry)
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

    /DISCARD/ :
    {
        *(.comment)
        *(.eh_frame)
        *(.note*)
    }
}
