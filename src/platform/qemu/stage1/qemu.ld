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

    .testelfblob :
    {
        __test_elf_blob_start = .;
        *(.testelfblob)
        __test_elf_blob_end = .;
    }

    . = ALIGN(16);

    __rom_free_first = .;
    __rom_free_end = 0x000F0000;
    ASSERT(__rom_free_first <= __rom_free_end, "ROM payloads overlap free area")

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

    ASSERT(. <= 0x000FFFF8, "reset vector overlaps ROM free descriptor")

    . = 0x000FFFF8;

    .rom_free_descriptor :
    {
        LONG(__rom_free_first + 0xFFF00000)
        LONG(__rom_free_end + 0xFFF00000)
    }

    /DISCARD/ :
    {
        *(.comment)
        *(.eh_frame)
        *(.note*)
    }
}
