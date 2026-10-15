/*
 * STM32CubeIDE 링커 스크립트(STM32H7xxxxTX_FLASH.ld)에 추가할 부분
 *
 * 1) MEMORY 블록에 RAM_D2 가 없으면 추가 (H743/H753/H750 : 288K, H723/H733 : 32K)
 *
 *      RAM_D2 (xrw) : ORIGIN = 0x30000000, LENGTH = 288K
 *
 * 2) SECTIONS 블록 안, "/DISCARD/" 바로 앞에 아래 .lwip_sec 를 붙여 넣기
 *
 *    0x30000000  Rx DMA 디스크립터  (4 x 24 B)
 *    0x30000080  Tx DMA 디스크립터  (4 x 24 B)
 *    0x30000100  Rx 버퍼 풀         (8 x 1568 B ≈ 12.3 KB → ~0x30003200)
 *    0x30004000  lwIP heap          (LWIP_RAM_HEAP_POINTER, MEM_SIZE 14 KB)
 *                → 링커가 배치하지 않음. lwipopts 의 주소를 직접 사용
 */

  .lwip_sec (NOLOAD) :
  {
    . = ABSOLUTE(0x30000000);
    *(.RxDescripSection)

    . = ABSOLUTE(0x30000080);
    *(.TxDescripSection)

    . = ABSOLUTE(0x30000100);
    *(.Rx_PoolSection)
  } >RAM_D2

  /* Rx 버퍼 풀이 lwIP heap(0x30004000)을 침범하면 링크 단계에서 에러 */
  ASSERT(ADDR(.lwip_sec) + SIZEOF(.lwip_sec) <= 0x30004000,
         "ETH Rx pool overlaps LWIP_RAM_HEAP_POINTER (0x30004000)")
