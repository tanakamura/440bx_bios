ENTRY(app_entry)

SECTIONS
{
  . = 0x00180000;

  .text : {
    *(.text.entry)
    *(.text*)
    *(.rodata*)
  }

  .data : { *(.data*) }
  .bss : { *(.bss*) *(COMMON) }
}
