OUTPUT_FORMAT(elf32-i386)
ENTRY(postcar_resume)
INCLUDE include/blob.ld

SECTIONS
{
    . = BIOS_LOAD_LINEAR;

    .entry :
    {
        KEEP(*(.entry))
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
        *(.thunk16_legacy)
    }

    __bss_start = .;
    .bss :
    {
        *(.bss)
        *(.bss.*)
        *(COMMON)
    }
    __bss_end = .;

    ASSERT(. <= BIOS_LOAD_LINEAR + BIOS_LOAD_CAPACITY, "stage3 image exceeds high DRAM stage3 window")

    /DISCARD/ :
    {
        *(.comment)
        *(.eh_frame)
        *(.note*)
    }
}
