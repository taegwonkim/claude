/*
 ******************************************************************************
 * STM32H563ZI 링커 스크립트 (완전 버전)
 *
 * 수정 내용 (STM32CubeIDE 기본 스크립트 대비):
 *   1. MEMORY 섹션에 RAM2 (SRAM2) 추가
 *   2. SECTIONS에 .dma_buf 추가 → SRAM2(Non-Cacheable MPU 영역)에 배치
 *
 * SRAM 맵 (STM32H563ZI):
 *   SRAM1 : 0x20000000, 192 KB  ← 일반 코드/데이터
 *   SRAM2 : 0x20030000,  64 KB  ← DMA 버퍼 전용 (MPU Non-Cacheable)
 *   SRAM3 : 0x20040000,  16 KB  ← 추가 데이터
 ******************************************************************************
 */

/* 진입점 */
ENTRY(Reset_Handler)

/* 힙/스택 크기 */
_Min_Heap_Size  = 0x2000;
_Min_Stack_Size = 0x1000;

/* ── 메모리 영역 정의 ────────────────────────────────────────────────────── */
MEMORY
{
  FLASH (rx)     : ORIGIN = 0x08000000, LENGTH = 2048K
  RAM   (xrw)    : ORIGIN = 0x20000000, LENGTH = 192K   /* SRAM1 */
  RAM2  (xrw)    : ORIGIN = 0x20030000, LENGTH = 64K    /* SRAM2 – DMA 버퍼 전용 */
}

/* ── 섹션 배치 ───────────────────────────────────────────────────────────── */
SECTIONS
{
  /* 벡터 테이블 */
  .isr_vector :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector))
    . = ALIGN(4);
  } >FLASH

  /* 코드 및 읽기 전용 데이터 */
  .text :
  {
    . = ALIGN(4);
    *(.text)
    *(.text*)
    *(.glue_7)
    *(.glue_7t)
    *(.eh_frame)
    KEEP (*(.init))
    KEEP (*(.fini))
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

  .preinit_array :
  {
    . = ALIGN(4);
    PROVIDE_HIDDEN (__preinit_array_start = .);
    KEEP (*(.preinit_array*))
    PROVIDE_HIDDEN (__preinit_array_end = .);
    . = ALIGN(4);
  } >FLASH

  .init_array :
  {
    . = ALIGN(4);
    PROVIDE_HIDDEN (__init_array_start = .);
    KEEP (*(SORT(.init_array.*)))
    KEEP (*(.init_array*))
    PROVIDE_HIDDEN (__init_array_end = .);
    . = ALIGN(4);
  } >FLASH

  .fini_array :
  {
    . = ALIGN(4);
    PROVIDE_HIDDEN (__fini_array_start = .);
    KEEP (*(SORT(.fini_array.*)))
    KEEP (*(.fini_array*))
    PROVIDE_HIDDEN (__fini_array_end = .);
    . = ALIGN(4);
  } >FLASH

  /* 초기화 데이터 (플래시에 저장, RAM으로 복사) */
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

  /* BSS (미초기화) */
  . = ALIGN(4);
  .bss :
  {
    _sbss = .;
    __bss_start__ = _sbss;
    *(.bss)
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    _ebss = .;
    __bss_end__ = _ebss;
  } >RAM

  /* ══════════════════════════════════════════════════════════════════════
   * DMA 버퍼 전용 섹션 – SRAM2(RAM2) 배치
   *
   * 목적: MPU Region 0 (0x20030000, 8KB, Non-Cacheable)과 일치하는
   *       영역에 DMA 버퍼를 배치하여 D-Cache 일관성 문제 해결.
   *
   * NOLOAD: 플래시에 초기화 이미지를 저장하지 않음 (런타임 초기화).
   *         DMA 버퍼는 사용 전 코드에서 명시적으로 초기화.
   *
   * 포함 변수:
   *   sd_wr_buf, sd_rd_buf (main.c)
   *   s_dma_buf, s_ring    (uart_gpdma.c)
   * ══════════════════════════════════════════════════════════════════════ */
  .dma_buf (NOLOAD) :
  {
    . = ALIGN(32);
    _sdma_buf = .;
    *(.dma_buf)
    *(.dma_buf*)
    . = ALIGN(32);
    _edma_buf = .;
  } >RAM2

  /* 힙 / 스택 (SRAM1 끝 쪽) */
  ._user_heap_stack :
  {
    . = ALIGN(8);
    PROVIDE ( end = . );
    PROVIDE ( _end = . );
    . = . + _Min_Heap_Size;
    . = . + _Min_Stack_Size;
    . = ALIGN(8);
  } >RAM

  /* 디버그 섹션 (삭제 가능) */
  /DISCARD/ :
  {
    libc.a ( * )
    libm.a ( * )
    libgcc.a ( * )
  }

  .ARM.attributes 0 : { *(.ARM.attributes) }
}
