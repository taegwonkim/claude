/**
 * linker_patch.ld
 * ─────────────────────────────────────────────────────────────────────────
 * STM32H563ZI 링커 스크립트 패치: DMA 버퍼 섹션 추가
 *
 * 사용법:
 *   STM32CubeIDE 가 생성한 STM32H563ZI_FLASH.ld 의
 *   BSS 섹션 직후에 아래 .dma_buffer 섹션을 삽입하십시오.
 *
 * 예시 (STM32H563ZI_FLASH.ld 중 해당 위치):
 *
 *   ...
 *   .bss :
 *   {
 *     ...
 *   } >RAM
 *   ↑ 이 줄 바로 다음에 아래 블록 삽입 ↑
 * ─────────────────────────────────────────────────────────────────────────
 */

/*
 * .dma_buffer : SD 카드 IDMA + UART GPDMA 전용 버퍼
 *
 *   - NOLOAD  : 플래시 공간을 차지하지 않음 (RAM-only 섹션)
 *   - ALIGN(32): D-Cache 라인(32바이트) 경계 정렬 보장
 *   - >RAM    : SRAM1 에 배치 (AHB 접근 가능, DMA 가시 영역)
 *
 * 소스 코드에서 사용 예:
 *   static uint8_t sd_buf[512]
 *       __attribute__((aligned(32), section(".DMABufferSection")));
 */

/* ── 링커 스크립트에 추가할 섹션 ────────────────────────────────────── */
/*
  .dma_buffer (NOLOAD) :
  {
    . = ALIGN(32);
    *(.DMABufferSection)
    *(.DMABufferSection*)
    . = ALIGN(32);
  } >RAM
*/

/**
 * 전체 STM32H563ZI_FLASH.ld 내 SECTIONS 블록 통합 예시
 * (STM32CubeIDE 기본 생성 파일 기준, 수정 위치만 표시):
 *
 *  SECTIONS
 *  {
 *    .isr_vector ...   /* 벡터 테이블 (플래시) */
 *    .text ...         /* 코드 (플래시) */
 *    .rodata ...       /* 읽기 전용 데이터 (플래시) */
 *    .data ...         /* 초기화 데이터 (RAM, 플래시에서 복사) */
 *    .bss ...          /* 미초기화 데이터 (RAM) */
 *
 *    ↓↓↓ 아래 섹션 추가 ↓↓↓
 *
 *    .dma_buffer (NOLOAD) :
 *    {
 *      . = ALIGN(32);
 *      *(.DMABufferSection)
 *      *(.DMABufferSection*)
 *      . = ALIGN(32);
 *    } >RAM
 *
 *    ↑↑↑ 여기까지 ↑↑↑
 *
 *    ._user_heap_stack ...
 *  }
 */
