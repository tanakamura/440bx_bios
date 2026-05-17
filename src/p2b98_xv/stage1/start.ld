OUTPUT_FORMAT(elf32-i386)
ENTRY(start)

SECTIONS
{
    . = 0x000C0000;

    .rom_anchor :
    {
        BYTE(0xC3)
    }

    . = 0x000C2000;

    .biosblob :
    {
        __bios_blob_start = .;
        *(.biosblob)
        __bios_blob_end = .;
    }

    . = ALIGN(16);

    .stage2blob :
    {
        __stage2_blob_start = .;
        *(.stage2blob)
        __stage2_blob_end = .;
    }

    . = ALIGN(16);

    .dsdtblob :
    {
        __dsdt_blob_start = .;
        *(.dsdtblob)
        __dsdt_blob_end = .;
    }

    . = ALIGN(16);

    .vgabiosblob :
    {
        __vgabios_blob_start = .;
        *(.vgabiosblob)
        __vgabios_blob_end = .;
    }

    . = ALIGN(16);

    .testelfblob :
    {
        __test_elf_blob_start = .;
        *(.testelfblob)
        __test_elf_blob_end = .;
    }

    . = ALIGN(16);

    .blobsvc :
    {
        __blob_service_start = .;
        KEEP(*(.blobsvc.entry))
        KEEP(*(.blobsvc))
        __blob_service_end = .;
    }

    . = 0x000FC000;

    .startdata :
    {
        KEEP(*(.startdata))
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

    .got :
    {
        *(.got)
        *(.got.*)
        *(.igot.plt)
        *(.got.plt)
    }

    .bss :
    {
        *(.bss)
        *(COMMON)
    }

    ASSERT(. <= 0x000FE000, "start ROM data exceeds 0xfc000-0xfdfff")

    . = 0x000FE000;

    .start :
    {
        KEEP(*(.start))
    }

    .text :
    {
        *(.text)
        *(.text.*)
        *(.iplt)
    }

    ASSERT(. <= 0x000FFFF0, "start ROM text overlaps reset vector")

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
