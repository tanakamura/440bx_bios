OUTPUT_FORMAT("elf32-i386")
ENTRY(app_entry)
INCLUDE include/blob.ld

PHDRS {
    text PT_LOAD FLAGS(5);
    data PT_LOAD FLAGS(6);
}

SECTIONS {
    . = APP_SLOT_LOAD_LINEAR;

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

    ASSERT(. <= APP_SLOT_LOAD_LINEAR + APP_SLOT_LOAD_CAPACITY, "legacy app exceeds app slot")

    /DISCARD/ : {
        *(.entry*)
        *(.comment*)
        *(.eh_frame*)
        *(.note*)
    }
}
