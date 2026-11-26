/*
 * CubeIDE가 생성한 STM32H743xx_FLASH.ld 에 추가할 블록입니다.
 *
 * 1) MEMORY 블록에 RAM_D2가 있는지 확인합니다(CubeIDE 기본 파일에는 보통 있습니다).
 *      RAM_D2 (xrw) : ORIGIN = 0x30000000, LENGTH = 288K
 *
 * 2) SECTIONS 안의 "/DISCARD/ :" 바로 앞에 아래 블록을 붙여 넣습니다.
 */
  .RAM_D2 (NOLOAD) :
  {
    . = ALIGN(32);
    *(.RAM_D2)
    *(.RAM_D2*)
    . = ALIGN(32);
  } >RAM_D2
