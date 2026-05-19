OUTPUT_FORMAT("elf32-i386")
ENTRY(legacy_app_entry)

PHDRS {
    text PT_LOAD FLAGS(5);
    data PT_LOAD FLAGS(6);
}

SECTIONS {
    . = 0x000F0000;

    .text : {
        *(.text*)
        *(.thunk16*)
    } :text

    .rodata : {
        *(.rodata*)
    } :text

    . = ALIGN(16);
    .data : {
        *(.data*)
    } :data

    .bss (NOLOAD) : {
        *(COMMON)
        *(.bss*)
    } :data

    /DISCARD/ : {
        *(.entry*)
        *(.comment*)
        *(.eh_frame*)
        *(.note*)
    }
}
