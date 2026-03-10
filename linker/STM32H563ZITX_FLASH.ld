/*
******************************************************************************
* STM32H563ZITX 커스텀 링커 스크립트
*
* 메모리 구성:
*   FLASH  : 2048KB @ 0x08000000  - 코드, 읽기전용 데이터
*   SRAM1  :  256KB @ 0x20000000  - 일반 RAM (스택, 힙, 데이터)
*   SRAM2  :   64KB @ 0x20040000  - DMA 버퍼, 고속 접근 데이터
*   BKPRAM :    4KB @ 0x40003C00  - 배터리 백업 RAM
*
* 섹션 배치:
*   .text          → FLASH  (코드)
*   .rodata        → FLASH  (상수)
*   .data          → SRAM1  (초기화된 전역변수, FLASH에서 복사)
*   .bss           → SRAM1  (초기화되지 않은 전역변수)
*   ._user_heap_stack → SRAM1 (힙 + 스택)
*   .sram2_data    → SRAM2  (DMA 버퍼, 명시적 배치 데이터)
*   .bkpram_data   → BKPRAM (배터리 백업 변수)
******************************************************************************
*/

/* 진입점 (Entry Point) */
ENTRY(Reset_Handler)

/* 스택 최상단 주소: SRAM1 끝 */
_estack = ORIGIN(SRAM1) + LENGTH(SRAM1);

/* 최소 힙/스택 크기 */
_Min_Heap_Size  = 0x800;   /* 2KB */
_Min_Stack_Size = 0x1000;  /* 4KB */

/* ============================================================
 * 메모리 영역 정의
 * ============================================================ */
MEMORY
{
  FLASH   (rx)  : ORIGIN = 0x08000000, LENGTH = 2048K
  SRAM1   (rwx) : ORIGIN = 0x20000000, LENGTH = 256K
  SRAM2   (rwx) : ORIGIN = 0x20040000, LENGTH = 64K
  BKPRAM  (rw)  : ORIGIN = 0x40003C00, LENGTH = 4K
}

/* ============================================================
 * 섹션 정의
 * ============================================================ */
SECTIONS
{
  /* ----------------------------------------------------------
   * .isr_vector: 인터럽트 벡터 테이블 (FLASH 최상단)
   * ---------------------------------------------------------- */
  .isr_vector :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector))
    . = ALIGN(4);
  } >FLASH

  /* ----------------------------------------------------------
   * .text: 프로그램 코드
   * ---------------------------------------------------------- */
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

  /* ----------------------------------------------------------
   * .rodata: 읽기 전용 데이터 (상수, 문자열 리터럴)
   * ---------------------------------------------------------- */
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

  /* C++ 생성자/소멸자 배열 */
  .preinit_array :
  {
    . = ALIGN(4);
    PROVIDE_HIDDEN (__preinit_array_start = .);
    KEEP(*(.preinit_array*))
    PROVIDE_HIDDEN (__preinit_array_end = .);
    . = ALIGN(4);
  } >FLASH

  .init_array :
  {
    . = ALIGN(4);
    PROVIDE_HIDDEN (__init_array_start = .);
    KEEP(*(SORT(.init_array.*)))
    KEEP(*(.init_array*))
    PROVIDE_HIDDEN (__init_array_end = .);
    . = ALIGN(4);
  } >FLASH

  .fini_array :
  {
    . = ALIGN(4);
    PROVIDE_HIDDEN (__fini_array_start = .);
    KEEP(*(SORT(.fini_array.*)))
    KEEP(*(.fini_array*))
    PROVIDE_HIDDEN (__fini_array_end = .);
    . = ALIGN(4);
  } >FLASH

  /* ----------------------------------------------------------
   * .data: 초기화된 전역/정적 변수
   * FLASH에 초기값 저장 → 부팅 시 SRAM1으로 복사
   * ---------------------------------------------------------- */
  _sidata = LOADADDR(.data);   /* FLASH 내 초기값 시작 주소 */

  .data :
  {
    . = ALIGN(4);
    _sdata = .;               /* SRAM1 데이터 시작 */
    *(.data)
    *(.data*)
    *(.RamFunc)               /* SRAM에서 실행할 함수 */
    *(.RamFunc*)
    . = ALIGN(4);
    _edata = .;               /* SRAM1 데이터 끝 */
  } >SRAM1 AT> FLASH          /* VMA: SRAM1, LMA: FLASH */

  /* ----------------------------------------------------------
   * .bss: 초기화되지 않은 전역/정적 변수 (0으로 초기화)
   * ---------------------------------------------------------- */
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
  } >SRAM1

  /* ----------------------------------------------------------
   * .sram1_data: 명시적 SRAM1 배치 섹션
   * 사용: __attribute__((section(".sram1_data")))
   * ---------------------------------------------------------- */
  _sisram1 = LOADADDR(.sram1_data);

  .sram1_data :
  {
    . = ALIGN(4);
    _ssram1 = .;
    *(.sram1_data)
    *(.sram1_data*)
    . = ALIGN(4);
    _esram1 = .;
  } >SRAM1 AT> FLASH

  /* ----------------------------------------------------------
   * .sram2_data: SRAM2 전용 섹션 (DMA 버퍼, Non-Cacheable)
   * 사용: __attribute__((section(".sram2_data")))
   *
   * 주의: 초기값이 필요한 경우 AT> FLASH 사용
   *       초기값 불필요(0 초기화)면 AT> 없이 사용
   * ---------------------------------------------------------- */
  _sisram2 = LOADADDR(.sram2_data);

  .sram2_data :
  {
    . = ALIGN(32);             /* 캐시 라인(32바이트) 정렬 */
    _ssram2 = .;
    *(.sram2_data)
    *(.sram2_data*)
    . = ALIGN(32);
    _esram2 = .;
  } >SRAM2 AT> FLASH

  /* SRAM2 BSS (초기화 불필요 섹션) */
  .sram2_bss (NOLOAD) :
  {
    . = ALIGN(32);
    _ssram2_bss = .;
    *(.sram2_bss)
    *(.sram2_bss*)
    . = ALIGN(32);
    _esram2_bss = .;
  } >SRAM2

  /* ----------------------------------------------------------
   * .bkpram_data: 배터리 백업 RAM 섹션
   * 사용: __attribute__((section(".bkpram_data")))
   *
   * 주의: NOLOAD - 부팅 시 초기화하지 않음 (백업 데이터 보존)
   *       전원 최초 인가 시 애플리케이션에서 직접 초기화 필요
   * ---------------------------------------------------------- */
  .bkpram_data (NOLOAD) :
  {
    . = ALIGN(4);
    _sbkpram = .;
    *(.bkpram_data)
    *(.bkpram_data*)
    . = ALIGN(4);
    _ebkpram = .;
  } >BKPRAM

  /* ----------------------------------------------------------
   * 힙(Heap) + 스택(Stack): SRAM1 상단
   * ---------------------------------------------------------- */
  ._user_heap_stack :
  {
    . = ALIGN(8);
    PROVIDE(end = .);          /* C 라이브러리 힙 끝 포인터 */
    PROVIDE(_end = .);
    . = . + _Min_Heap_Size;
    . = . + _Min_Stack_Size;
    . = ALIGN(8);
  } >SRAM1

  /* 사용하지 않는 섹션 제거 */
  /DISCARD/ :
  {
    libc.a(*)
    libm.a(*)
    libgcc.a(*)
  }

  .ARM.attributes 0 : { *(.ARM.attributes) }
}

/* ============================================================
 * 각 RAM 영역 사용량 확인용 심볼
 * ============================================================ */
_sram1_used  = _ebss - ORIGIN(SRAM1);
_sram2_used  = _esram2 - ORIGIN(SRAM2);
_bkpram_used = _ebkpram - ORIGIN(BKPRAM);
