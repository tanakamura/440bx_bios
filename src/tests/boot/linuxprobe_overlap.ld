ENTRY(_start)

PHDRS {
    text PT_LOAD FLAGS(5);
    defer PT_LOAD FLAGS(4);
}

SECTIONS {
    . = 0x00100000;
    .text : {
        *(.text*)
        *(.rodata*)
    } :text
    .data : { *(.data*) } :text
    .bss : { *(.bss*) *(COMMON) } :text

    . = 0x00200000;
    .defer : {
        *(.defer*)
    } :defer
}
