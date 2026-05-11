OUTPUT_FORMAT(elf32-i386)
ENTRY(qemu_start)

SECTIONS
{
    . = 0x000C0000;

    .rom_anchor :
    {
        BYTE(0xC3)
    }

    . = ALIGN(16);

    .fdos :
    {
        __fdos_blob_start = .;
        *(.fdos)
        __fdos_blob_end = .;
    }

    . = ALIGN(16);

    .biosblob :
    {
        __bios_blob_start = .;
        *(.biosblob)
        __bios_blob_end = .;
    }

    . = ALIGN(16);

    .blobsvc :
    {
        __blob_service_start = .;
        KEEP(*(.blobsvc.entry))
        KEEP(*(.blobsvc))
        __blob_service_end = .;
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
