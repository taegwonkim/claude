/*
 * STM32L552RETX_FLASH.ld - 링커 스크립트
 *
 * STM32L552RET6 메모리 맵:
 *   Flash  : 512KB @ 0x08000000
 *   SRAM1  : 192KB @ 0x20000000  (주 RAM)
 *   SRAM2  :  64KB @ 0x10000000  (보조 RAM, 선택 사용)
 */

ENTRY(Reset_Handler)

/* 스택 및 힙 최솟값 */
_Min_Heap_Size  = 0x800;    /* 2KB 힙 최소값 */
_Min_Stack_Size = 0x800;    /* 2KB 스택 최소값 */

MEMORY
{
  FLASH  (rx)  : ORIGIN = 0x08000000, LENGTH = 512K
  RAM    (xrw) : ORIGIN = 0x20000000, LENGTH = 192K
  RAM2   (xrw) : ORIGIN = 0x10000000, LENGTH = 64K
}

SECTIONS
{
  /* =========================================================
   * .isr_vector: 벡터 테이블 → Flash 시작
   * ========================================================= */
  .isr_vector :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector))
    . = ALIGN(4);
  } >FLASH

  /* =========================================================
   * .text: 코드 및 읽기 전용 데이터
   * ========================================================= */
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

  /* =========================================================
   * .rodata: 상수 데이터
   * ========================================================= */
  .rodata :
  {
    . = ALIGN(4);
    *(.rodata)
    *(.rodata*)
    . = ALIGN(4);
  } >FLASH

  /* ARM 예외 처리 테이블 */
  .ARM.extab :
  {
    . = ALIGN(4);
    *(.ARM.extab* .gnu.linkonce.armextab.*)
    . = ALIGN(4);
  } >FLASH

  .ARM :
  {
    . = ALIGN(4);
    __exidx_start = .;
    *(.ARM.exidx*)
    __exidx_end = .;
    . = ALIGN(4);
  } >FLASH

  /* C++ 생성자/소멸자 테이블 */
  .preinit_array :
  {
    . = ALIGN(4);
    PROVIDE_HIDDEN(__preinit_array_start = .);
    KEEP(*(.preinit_array*))
    PROVIDE_HIDDEN(__preinit_array_end = .);
    . = ALIGN(4);
  } >FLASH

  .init_array :
  {
    . = ALIGN(4);
    PROVIDE_HIDDEN(__init_array_start = .);
    KEEP(*(SORT(.init_array.*)))
    KEEP(*(.init_array*))
    PROVIDE_HIDDEN(__init_array_end = .);
    . = ALIGN(4);
  } >FLASH

  .fini_array :
  {
    . = ALIGN(4);
    PROVIDE_HIDDEN(__fini_array_start = .);
    KEEP(*(SORT(.fini_array.*)))
    KEEP(*(.fini_array*))
    PROVIDE_HIDDEN(__fini_array_end = .);
    . = ALIGN(4);
  } >FLASH

  /* =========================================================
   * .data: 초기화된 전역/정적 변수 (Flash → RAM 복사)
   * ========================================================= */
  _sidata = LOADADDR(.data);

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

  /* =========================================================
   * .bss: 초기화되지 않은 전역/정적 변수 (0으로 초기화)
   * ========================================================= */
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

  /* =========================================================
   * 사용자 힙 및 스택 크기 검증
   * ========================================================= */
  ._user_heap_stack :
  {
    . = ALIGN(8);
    PROVIDE(end = .);
    PROVIDE(_end = .);
    . = . + _Min_Heap_Size;
    . = . + _Min_Stack_Size;
    . = ALIGN(8);
  } >RAM

  /* =========================================================
   * SRAM2 에 배치할 섹션 (DMA 버퍼 등, 선택 사용)
   *   변수에 __attribute__((section(".sram2"))) 으로 지정
   * ========================================================= */
  .sram2 (NOLOAD) :
  {
    . = ALIGN(4);
    *(.sram2)
    *(.sram2*)
    . = ALIGN(4);
  } >RAM2

  /* 디버그 정보 */
  /DISCARD/ :
  {
    libc.a ( * )
    libm.a ( * )
    libgcc.a ( * )
  }

  .ARM.attributes 0 : { *(.ARM.attributes) }
}
