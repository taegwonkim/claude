/*
 * Linker script: STM32L552RCT6  (256KB Flash, 256KB RAM)
 *
 * EEPROM 에뮬레이션 영역 (32KB = 4 pages × 2KB × 4)을
 * 코드 영역에서 분리하여 링커가 덮어쓰지 않도록 합니다.
 *
 * eeprom_emul_conf.h 와 반드시 일치해야 합니다:
 *   EEPROM_START_ADDRESS = 0x08038000
 *   NB_OF_PAGES          = 4
 */

ENTRY(Reset_Handler)

_estack = ORIGIN(RAM) + LENGTH(RAM);  /* top of stack */

_Min_Heap_Size  = 0x400;   /* 1KB minimum heap  */
_Min_Stack_Size = 0x800;   /* 2KB minimum stack */

MEMORY
{
  /* Flash: 0x08000000 ~ 0x08037FFF (224KB) — application code + data */
  FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 224K

  /* EEPROM emulation: 0x08038000 ~ 0x0803FFFF (32KB) — DO NOT use for code */
  /* EEPROM (r) : ORIGIN = 0x08038000, LENGTH = 32K */  /* informational only */

  /* SRAM1 + SRAM2 = 256KB total */
  RAM   (xrw) : ORIGIN = 0x20000000, LENGTH = 256K
}

SECTIONS
{
  .isr_vector :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector))
    . = ALIGN(4);
  } >FLASH

  .text :
  {
    . = ALIGN(4);
    *(.text)
    *(.text*)
    *(.glue_7)
    *(.glue_7t)
    *(.eh_frame)
    KEEP(*(.init))
    KEEP(*(.fini))
    . = ALIGN(4);
    _etext = .;
  } >FLASH

  .rodata :
  {
    . = ALIGN(4);
    *(.rodata)
    *(.rodata*)
    . = ALIGN(4);
  } >FLASH

  .ARM.extab : { *(.ARM.extab* .gnu.linkonce.armextab.*) } >FLASH
  .ARM        : { __exidx_start = .; *(.ARM.exidx*) __exidx_end = .; } >FLASH

  .preinit_array :
  {
    PROVIDE_HIDDEN(__preinit_array_start = .);
    KEEP(*(.preinit_array*))
    PROVIDE_HIDDEN(__preinit_array_end = .);
  } >FLASH

  .init_array :
  {
    PROVIDE_HIDDEN(__init_array_start = .);
    KEEP(*(SORT(.init_array.*)))
    KEEP(*(.init_array*))
    PROVIDE_HIDDEN(__init_array_end = .);
  } >FLASH

  .fini_array :
  {
    PROVIDE_HIDDEN(__fini_array_start = .);
    KEEP(*(SORT(.fini_array.*)))
    KEEP(*(.fini_array*))
    PROVIDE_HIDDEN(__fini_array_end = .);
  } >FLASH

  _sidata = LOADADDR(.data);

  .data :
  {
    . = ALIGN(4);
    _sdata = .;
    *(.data)
    *(.data*)
    . = ALIGN(4);
    _edata = .;
  } >RAM AT> FLASH

  .bss :
  {
    . = ALIGN(4);
    _sbss = .;
    __bss_start__ = _sbss;
    *(.bss)
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    _ebss = .;
    __bss_end__ = _ebss;
  } >RAM

  ._user_heap_stack :
  {
    . = ALIGN(8);
    PROVIDE(end = .);
    PROVIDE(_end = .);
    . = . + _Min_Heap_Size;
    . = . + _Min_Stack_Size;
    . = ALIGN(8);
  } >RAM

  /DISCARD/ : { libc.a ( * ) libm.a ( * ) libgcc.a ( * ) }

  .ARM.attributes 0 : { *(.ARM.attributes) }
}
