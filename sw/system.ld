SECTIONS
{
  . = 0x00000000;
  .ROM :
  {
    start*(.text)
    *(.text)
    *(.text.*)
    *(.rodata)
    *(.rodata.*)
  }

  . = 0x10000000;
  .RAM :
  {
    *(.bss)
    *(.data)
  }
}
