/**
 ******************************************************************************
 * @file    STM32L552RETx_FLASH.ld
 * @brief   STM32L552RET6 링커 스크립트 (Non-Secure, TrustZone 미사용)
 *
 * Memory Map:
 *   Flash : 512KB @ 0x08000000
 *   SRAM1 : 192KB @ 0x20000000
 *   SRAM2 :  64KB @ 0x20030000
 ******************************************************************************
 */

/* Entry Point */
ENTRY(Reset_Handler)

/* Highest address of the user mode stack */
_estack = ORIGIN(RAM) + LENGTH(RAM);  /* end of RAM */

/* Minimum heap and stack sizes */
_Min_Heap_Size  = 0x400;   /* 1KB  (FreeRTOS는 자체 힙 사용) */
_Min_Stack_Size = 0x800;   /* 2KB  (ISR 스택) */

/* Memory Definitions */
MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 512K
    RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 192K
    SRAM2 (rwx) : ORIGIN = 0x20030000, LENGTH = 64K
}

/* Sections */
SECTIONS
{
    /* Interrupt vector table - must be at the start of flash */
    .isr_vector :
    {
        . = ALIGN(4);
        KEEP(*(.isr_vector))
        . = ALIGN(4);
    } >FLASH

    /* Program code and constants */
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

    /* Read-only data */
    .rodata :
    {
        . = ALIGN(4);
        *(.rodata)
        *(.rodata*)
        . = ALIGN(4);
    } >FLASH

    /* ARM exception handling */
    .ARM.extab :
    {
        *(.ARM.extab* .gnu.linkonce.armextab.*)
    } >FLASH

    .ARM :
    {
        __exidx_start = .;
        *(.ARM.exidx*)
        __exidx_end = .;
    } >FLASH

    /* Pre-init and init/fini arrays */
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

    /* Used by startup to initialize data */
    _sidata = LOADADDR(.data);

    /* Initialized data (copied from flash to RAM at startup) */
    .data :
    {
        . = ALIGN(4);
        _sdata = .;
        *(.data)
        *(.data*)
        *(.RamFunc)
        *(.RamFunc*)
        . = ALIGN(4);
        _edata = .;
    } >RAM AT> FLASH

    /* Uninitialized data (zeroed at startup) */
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

    /* User heap and stack check */
    ._user_heap_stack :
    {
        . = ALIGN(8);
        PROVIDE(end = .);
        PROVIDE(_end = .);
        . = . + _Min_Heap_Size;
        . = . + _Min_Stack_Size;
        . = ALIGN(8);
    } >RAM

    /* Remove debug info from standard libraries */
    /DISCARD/ :
    {
        libc.a(*)
        libm.a(*)
        libgcc.a(*)
    }

    .ARM.attributes 0 : { *(.ARM.attributes) }
}
