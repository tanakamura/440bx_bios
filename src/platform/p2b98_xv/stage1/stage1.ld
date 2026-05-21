OUTPUT_FORMAT(elf32-i386)
ENTRY(start)
INCLUDE shared_service/service_table.ld

SECTIONS
{
    . = SHARED_ROM_LOW_BASE;

    .rom_anchor :
    {
        BYTE(0xC3)
    }

    . = SHARED_ROM_LOW_BASE + 0x2000;

    __rom_free_first = .;
    __rom_free_end = 0x000FC000;
    ASSERT(__rom_free_first <= __rom_free_end, "ROM payloads overlap free area")

    . = 0x000FC000;

    .startdata :
    {
        KEEP(*(.startdata))
    }

    .blobsvc :
    {
        __blob_service_start = .;
        KEEP(*(.blobsvc.entry))
        KEEP(*(.blobsvc))
        __blob_service_end = .;
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
        *(.bss.*)
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

    ASSERT(. <= 0x000FFFF8, "reset vector overlaps ROM free descriptor")

    . = 0x000FFFF8;

    .rom_free_descriptor :
    {
        LONG(__rom_free_first + SHARED_ROM_HIGH_DELTA)
        LONG(__rom_free_end + SHARED_ROM_HIGH_DELTA)
    }

    /DISCARD/ :
    {
        *(.comment)
        *(.eh_frame)
        *(.note*)
    }
}
