OUTPUT_FORMAT(elf32-i386)
ENTRY(qemu_stage2_entry)

SECTIONS
{
    . = 0x00080000;

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

    ASSERT(. <= 0x00090000, "qemu stage2 exceeds 0x80000-0x8ffff")

    /DISCARD/ :
    {
        *(.comment)
        *(.eh_frame)
        *(.note*)
    }
}
