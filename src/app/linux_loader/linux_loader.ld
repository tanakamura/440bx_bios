OUTPUT_FORMAT("elf32-i386")
ENTRY(linux_loader_app_entry)
INCLUDE include/blob.ld

PHDRS {
    text PT_LOAD FLAGS(5);
    data PT_LOAD FLAGS(6);
}

SECTIONS {
    . = APP_SLOT_LOAD_LINEAR;

    .text : {
        *(.text*)
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

    ASSERT(. <= APP_SLOT_LOAD_LINEAR + APP_SLOT_LOAD_CAPACITY, "linux loader app exceeds app slot")

    /DISCARD/ : {
        *(.comment*)
        *(.eh_frame*)
        *(.note*)
    }
}
