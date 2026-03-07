/*
 * 링커 스크립트 — STM32H563ZITx (2MB Flash, 640KB SRAM)
 *
 * 메모리 맵
 * ----------
 * FLASH  : 0x0800_0000  2048KB
 * SRAM1  : 0x2000_0000   256KB  (AHB SRAM1 — DMA 접근 가능)
 * SRAM2  : 0x2004_0000   256KB  (AHB SRAM2 — DMA 접근 가능)
 * SRAM3  : 0x2008_0000   512KB  (AHB SRAM3)  ← 필요 시 사용
 *
 * DMA 버퍼는 반드시 SRAM1/SRAM2 범위에 위치해야 GPDMA가 접근 가능.
 * CCM(Core Coupled Memory)은 STM32H5에 없음.
 */

ENTRY(Reset_Handler)

_estack = ORIGIN(RAM) + LENGTH(RAM);   /* 스택 최상위 주소 */

_Min_Heap_Size  = 0x1000;   /* 4KB 힙 */
_Min_Stack_Size = 0x800;    /* 2KB 스택 */

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 2048K
    RAM   (xrw) : ORIGIN = 0x20000000, LENGTH = 256K    /* SRAM1 */
    RAM2  (xrw) : ORIGIN = 0x20040000, LENGTH = 256K    /* SRAM2 */
}

SECTIONS
{
    /* -------- 벡터 테이블 & 코드 -------- */
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
    .ARM        : { __exidx_start = .; *(.ARM.exidx*); __exidx_end = .; } >FLASH

    .preinit_array :
    {
        PROVIDE_HIDDEN (__preinit_array_start = .);
        KEEP(*(.preinit_array*))
        PROVIDE_HIDDEN (__preinit_array_end = .);
    } >FLASH

    .init_array :
    {
        PROVIDE_HIDDEN (__init_array_start = .);
        KEEP(*(SORT(.init_array.*)))
        KEEP(*(.init_array*))
        PROVIDE_HIDDEN (__init_array_end = .);
    } >FLASH

    .fini_array :
    {
        PROVIDE_HIDDEN (__fini_array_start = .);
        KEEP(*(SORT(.fini_array.*)))
        KEEP(*(.fini_array*))
        PROVIDE_HIDDEN (__fini_array_end = .);
    } >FLASH

    /* -------- 초기화 데이터 (.data) -------- */
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

    /* -------- DMA 버퍼 전용 섹션 (SRAM1 — no-cache 가능) -------- */
    /*
     * .dma_buffer 섹션을 별도로 정의하면 SCB_CleanDCache / Invalidate 범위를
     * 명확히 지정할 수 있다. 버퍼 선언 예:
     *   __attribute__((section(".dma_buffer"), aligned(32))) uint8_t buf[512];
     */
    .dma_buffer (NOLOAD) :
    {
        . = ALIGN(32);
        _sdma_buffer = .;
        *(.dma_buffer)
        *(.dma_buffer*)
        . = ALIGN(32);
        _edma_buffer = .;
    } >RAM

    /* -------- BSS (0으로 초기화) -------- */
    .bss :
    {
        . = ALIGN(4);
        _sbss = .;
        *(.bss)
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
    } >RAM

    /* -------- 힙/스택 영역 -------- */
    ._user_heap_stack :
    {
        . = ALIGN(8);
        PROVIDE ( end = . );
        PROVIDE ( _end = . );
        . = . + _Min_Heap_Size;
        . = . + _Min_Stack_Size;
        . = ALIGN(8);
    } >RAM

    /* SRAM2: FatFS 작업 버퍼 등 대용량 임시 버퍼에 활용 */
    .sram2 (NOLOAD) :
    {
        . = ALIGN(4);
        _ssram2 = .;
        *(.sram2)
        *(.sram2*)
        . = ALIGN(4);
        _esram2 = .;
    } >RAM2

    /DISCARD/ :
    {
        libc.a ( * )
        libm.a ( * )
        libgcc.a ( * )
    }

    .ARM.attributes 0 : { *(.ARM.attributes) }
}
