/**
  ******************************************************************************
  * @file    startup_stm32h563xx.s
  * @brief   STM32H563xx 시작 코드 (GNU 어셈블리)
  *          - 스택/힙 초기화
  *          - .data 섹션 RAM 복사
  *          - .bss 섹션 초기화
  *          - SystemInit → main 호출
  ******************************************************************************
  */

  .syntax unified
  .cpu cortex-m33
  .fpu fpv5-sp-d16
  .thumb

  .global g_pfnVectors
  .global Default_Handler

/* 스택 시작 주소 (링커 스크립트에서 정의) */
.word _estack

/* ============================================================================
 *  Reset 핸들러
 * ============================================================================ */
  .section .text.Reset_Handler
  .weak Reset_Handler
  .type Reset_Handler, %function
Reset_Handler:
  ldr   r0, =_estack
  mov   sp, r0              /* 스택 포인터 초기화 */

  /* .data 섹션: FLASH → RAM 복사 */
  ldr   r0, =_sdata
  ldr   r1, =_edata
  ldr   r2, =_sidata
  movs  r3, #0
  b     LoopCopyDataInit

CopyDataInit:
  ldr   r4, [r2, r3]
  str   r4, [r0, r3]
  adds  r3, r3, #4

LoopCopyDataInit:
  adds  r4, r0, r3
  cmp   r4, r1
  bcc   CopyDataInit

  /* .bss 섹션: 0으로 초기화 */
  ldr   r2, =_sbss
  ldr   r4, =_ebss
  movs  r3, #0
  b     LoopFillZerobss

FillZerobss:
  str   r3, [r2]
  adds  r2, r2, #4

LoopFillZerobss:
  cmp   r2, r4
  bcc   FillZerobss

  /* SystemInit 호출 (클록은 main에서 설정하므로 여기선 최소 초기화) */
  bl    SystemInit

  /* main 호출 */
  bl    main

  /* main이 리턴되면 무한 루프 */
LoopForever:
  b     LoopForever

  .size Reset_Handler, .-Reset_Handler

/* ============================================================================
 *  Default 핸들러
 * ============================================================================ */
  .section .text.Default_Handler,"ax",%progbits
Default_Handler:
  b     Default_Handler
  .size Default_Handler, .-Default_Handler

/* ============================================================================
 *  인터럽트 벡터 테이블
 * ============================================================================ */
  .section .isr_vector,"a",%progbits
  .type g_pfnVectors, %object
  .size g_pfnVectors, .-g_pfnVectors

g_pfnVectors:
  /* Cortex-M33 코어 예외 벡터 */
  .word  _estack
  .word  Reset_Handler
  .word  NMI_Handler
  .word  HardFault_Handler
  .word  MemManage_Handler
  .word  BusFault_Handler
  .word  UsageFault_Handler
  .word  SecureFault_Handler
  .word  0
  .word  0
  .word  0
  .word  SVC_Handler
  .word  DebugMon_Handler
  .word  0
  .word  PendSV_Handler
  .word  SysTick_Handler

  /* STM32H563xx 외부 인터럽트 벡터 (Position 0~) */
  .word  WWDG_IRQHandler
  .word  PVD_AVD_IRQHandler
  .word  RTC_IRQHandler
  .word  RTC_S_IRQHandler
  .word  TAMP_IRQHandler
  .word  RAMCFG_IRQHandler
  .word  FLASH_IRQHandler
  .word  FLASH_S_IRQHandler
  .word  GTZC_IRQHandler
  .word  RCC_IRQHandler
  .word  RCC_S_IRQHandler
  .word  EXTI0_IRQHandler
  .word  EXTI1_IRQHandler
  .word  EXTI2_IRQHandler
  .word  EXTI3_IRQHandler
  .word  EXTI4_IRQHandler
  .word  EXTI5_IRQHandler
  .word  EXTI6_IRQHandler
  .word  EXTI7_IRQHandler
  .word  EXTI8_IRQHandler
  .word  EXTI9_IRQHandler
  .word  EXTI10_IRQHandler
  .word  EXTI11_IRQHandler
  .word  EXTI12_IRQHandler
  .word  EXTI13_IRQHandler
  .word  EXTI14_IRQHandler
  .word  EXTI15_IRQHandler
  .word  GPDMA1_Channel0_IRQHandler   /* SDMMC1 TX (GPDMA1 CH0) */
  .word  GPDMA1_Channel1_IRQHandler   /* SDMMC1 RX (GPDMA1 CH1) */
  .word  GPDMA1_Channel2_IRQHandler
  .word  GPDMA1_Channel3_IRQHandler
  .word  GPDMA1_Channel4_IRQHandler
  .word  GPDMA1_Channel5_IRQHandler
  .word  GPDMA1_Channel6_IRQHandler
  .word  GPDMA1_Channel7_IRQHandler
  .word  IWDG_IRQHandler
  .word  ADC1_IRQHandler
  .word  DAC1_IRQHandler
  .word  FDCAN1_IT0_IRQHandler
  .word  FDCAN1_IT1_IRQHandler
  .word  TIM1_BRK_IRQHandler
  .word  TIM1_UP_IRQHandler
  .word  TIM1_TRG_COM_IRQHandler
  .word  TIM1_CC_IRQHandler
  .word  TIM2_IRQHandler
  .word  TIM3_IRQHandler
  .word  TIM4_IRQHandler
  .word  TIM5_IRQHandler
  .word  TIM6_IRQHandler
  .word  TIM7_IRQHandler
  .word  TIM8_BRK_IRQHandler
  .word  TIM8_UP_IRQHandler
  .word  TIM8_TRG_COM_IRQHandler
  .word  TIM8_CC_IRQHandler
  .word  I2C1_EV_IRQHandler
  .word  I2C1_ER_IRQHandler
  .word  I2C2_EV_IRQHandler
  .word  I2C2_ER_IRQHandler
  .word  SPI1_IRQHandler
  .word  SPI2_IRQHandler
  .word  SPI3_IRQHandler
  .word  USART1_IRQHandler            /* USART1 (GPDMA2 CH0/1) */
  .word  USART2_IRQHandler
  .word  USART3_IRQHandler
  .word  UART4_IRQHandler
  .word  UART5_IRQHandler
  .word  LPUART1_IRQHandler
  .word  LPTIM1_IRQHandler
  .word  LPTIM2_IRQHandler
  .word  TIM15_IRQHandler
  .word  TIM16_IRQHandler
  .word  TIM17_IRQHandler
  .word  COMP_IRQHandler
  .word  OTG_FS_IRQHandler
  .word  CRS_IRQHandler
  .word  FMC_IRQHandler
  .word  OCTOSPI1_IRQHandler
  .word  PWR_S3WU_IRQHandler
  .word  SDMMC1_IRQHandler            /* SDMMC1 */
  .word  SDMMC2_IRQHandler
  .word  GPDMA1_Channel8_IRQHandler
  .word  GPDMA1_Channel9_IRQHandler
  .word  GPDMA1_Channel10_IRQHandler
  .word  GPDMA1_Channel11_IRQHandler
  .word  GPDMA1_Channel12_IRQHandler
  .word  GPDMA1_Channel13_IRQHandler
  .word  GPDMA1_Channel14_IRQHandler
  .word  GPDMA1_Channel15_IRQHandler
  .word  I2C3_EV_IRQHandler
  .word  I2C3_ER_IRQHandler
  .word  SAI1_IRQHandler
  .word  SAI2_IRQHandler
  .word  TSC_IRQHandler
  .word  AES_IRQHandler
  .word  RNG_IRQHandler
  .word  FPU_IRQHandler
  .word  HASH_IRQHandler
  .word  PKA_IRQHandler
  .word  LPTIM3_IRQHandler
  .word  SPI4_IRQHandler
  .word  SPI5_IRQHandler
  .word  SPI6_IRQHandler
  .word  USART6_IRQHandler
  .word  USART10_IRQHandler
  .word  USART11_IRQHandler
  .word  UART7_IRQHandler
  .word  UART8_IRQHandler
  .word  UART9_IRQHandler
  .word  UART12_IRQHandler
  .word  SDMMC1_IRQHandler            /* 추가 SDMMC1 벡터 (H5 특수) */
  .word  GPDMA2_Channel0_IRQHandler   /* USART1 TX (GPDMA2 CH0) */
  .word  GPDMA2_Channel1_IRQHandler   /* USART1 RX (GPDMA2 CH1) */
  .word  GPDMA2_Channel2_IRQHandler
  .word  GPDMA2_Channel3_IRQHandler
  .word  GPDMA2_Channel4_IRQHandler
  .word  GPDMA2_Channel5_IRQHandler
  .word  GPDMA2_Channel6_IRQHandler
  .word  GPDMA2_Channel7_IRQHandler
  .word  DCMI_PSSI_IRQHandler
  .word  FDCAN2_IT0_IRQHandler
  .word  FDCAN2_IT1_IRQHandler
  .word  CORDIC_IRQHandler
  .word  FMAC_IRQHandler
  .word  GPDMA2_Channel8_IRQHandler
  .word  GPDMA2_Channel9_IRQHandler
  .word  GPDMA2_Channel10_IRQHandler
  .word  GPDMA2_Channel11_IRQHandler
  .word  GPDMA2_Channel12_IRQHandler
  .word  GPDMA2_Channel13_IRQHandler
  .word  GPDMA2_Channel14_IRQHandler
  .word  GPDMA2_Channel15_IRQHandler
  .word  ICACHE_IRQHandler
  .word  OTFDEC1_IRQHandler
  .word  LTDC_IRQHandler
  .word  LTDC_ER_IRQHandler
  .word  DMA2D_IRQHandler
  .word  JPEG_IRQHandler
  .word  GFXMMU_IRQHandler
  .word  I3C1_EV_IRQHandler
  .word  I3C1_ER_IRQHandler
  .word  I3C2_EV_IRQHandler
  .word  I3C2_ER_IRQHandler
  .word  MCE1_IRQHandler
  .word  MCE2_IRQHandler
  .word  MCE3_IRQHandler
  .word  XSPI1_IRQHandler
  .word  XSPI2_IRQHandler
  .word  PWR_WKUP_IRQHandler
  .word  GPU2D_IRQHandler
  .word  GPU2D_ER_IRQHandler

/* ============================================================================
 *  Weak 기본 핸들러 별칭
 *  사용자 코드에서 오버라이드 가능
 * ============================================================================ */
  .weak  NMI_Handler
  .thumb_set NMI_Handler, Default_Handler

  .weak  HardFault_Handler
  .thumb_set HardFault_Handler, Default_Handler

  .weak  MemManage_Handler
  .thumb_set MemManage_Handler, Default_Handler

  .weak  BusFault_Handler
  .thumb_set BusFault_Handler, Default_Handler

  .weak  UsageFault_Handler
  .thumb_set UsageFault_Handler, Default_Handler

  .weak  SecureFault_Handler
  .thumb_set SecureFault_Handler, Default_Handler

  .weak  SVC_Handler
  .thumb_set SVC_Handler, Default_Handler

  .weak  DebugMon_Handler
  .thumb_set DebugMon_Handler, Default_Handler

  .weak  PendSV_Handler
  .thumb_set PendSV_Handler, Default_Handler

  .weak  SysTick_Handler
  .thumb_set SysTick_Handler, Default_Handler

  /* GPDMA1 */
  .weak  GPDMA1_Channel0_IRQHandler
  .thumb_set GPDMA1_Channel0_IRQHandler, Default_Handler

  .weak  GPDMA1_Channel1_IRQHandler
  .thumb_set GPDMA1_Channel1_IRQHandler, Default_Handler

  .weak  GPDMA1_Channel2_IRQHandler
  .thumb_set GPDMA1_Channel2_IRQHandler, Default_Handler

  .weak  GPDMA1_Channel3_IRQHandler
  .thumb_set GPDMA1_Channel3_IRQHandler, Default_Handler

  .weak  GPDMA1_Channel4_IRQHandler
  .thumb_set GPDMA1_Channel4_IRQHandler, Default_Handler

  .weak  GPDMA1_Channel5_IRQHandler
  .thumb_set GPDMA1_Channel5_IRQHandler, Default_Handler

  .weak  GPDMA1_Channel6_IRQHandler
  .thumb_set GPDMA1_Channel6_IRQHandler, Default_Handler

  .weak  GPDMA1_Channel7_IRQHandler
  .thumb_set GPDMA1_Channel7_IRQHandler, Default_Handler

  /* GPDMA2 */
  .weak  GPDMA2_Channel0_IRQHandler
  .thumb_set GPDMA2_Channel0_IRQHandler, Default_Handler

  .weak  GPDMA2_Channel1_IRQHandler
  .thumb_set GPDMA2_Channel1_IRQHandler, Default_Handler

  .weak  GPDMA2_Channel2_IRQHandler
  .thumb_set GPDMA2_Channel2_IRQHandler, Default_Handler

  .weak  GPDMA2_Channel3_IRQHandler
  .thumb_set GPDMA2_Channel3_IRQHandler, Default_Handler

  .weak  GPDMA2_Channel4_IRQHandler
  .thumb_set GPDMA2_Channel4_IRQHandler, Default_Handler

  .weak  GPDMA2_Channel5_IRQHandler
  .thumb_set GPDMA2_Channel5_IRQHandler, Default_Handler

  .weak  GPDMA2_Channel6_IRQHandler
  .thumb_set GPDMA2_Channel6_IRQHandler, Default_Handler

  .weak  GPDMA2_Channel7_IRQHandler
  .thumb_set GPDMA2_Channel7_IRQHandler, Default_Handler

  /* SDMMC */
  .weak  SDMMC1_IRQHandler
  .thumb_set SDMMC1_IRQHandler, Default_Handler

  .weak  SDMMC2_IRQHandler
  .thumb_set SDMMC2_IRQHandler, Default_Handler

  /* USART/UART */
  .weak  USART1_IRQHandler
  .thumb_set USART1_IRQHandler, Default_Handler

  .weak  USART2_IRQHandler
  .thumb_set USART2_IRQHandler, Default_Handler

  .weak  USART3_IRQHandler
  .thumb_set USART3_IRQHandler, Default_Handler

  .weak  UART4_IRQHandler
  .thumb_set UART4_IRQHandler, Default_Handler

  .weak  UART5_IRQHandler
  .thumb_set UART5_IRQHandler, Default_Handler

  /* 기타 (필요에 따라 추가) */
  .weak  WWDG_IRQHandler
  .thumb_set WWDG_IRQHandler, Default_Handler

  .weak  RCC_IRQHandler
  .thumb_set RCC_IRQHandler, Default_Handler

  .weak  RNG_IRQHandler
  .thumb_set RNG_IRQHandler, Default_Handler

  .weak  TIM2_IRQHandler
  .thumb_set TIM2_IRQHandler, Default_Handler

  .weak  TIM3_IRQHandler
  .thumb_set TIM3_IRQHandler, Default_Handler

  .weak  SPI1_IRQHandler
  .thumb_set SPI1_IRQHandler, Default_Handler

  .weak  I2C1_EV_IRQHandler
  .thumb_set I2C1_EV_IRQHandler, Default_Handler

  .weak  I2C1_ER_IRQHandler
  .thumb_set I2C1_ER_IRQHandler, Default_Handler

  .weak  ICACHE_IRQHandler
  .thumb_set ICACHE_IRQHandler, Default_Handler

  .weak  FPU_IRQHandler
  .thumb_set FPU_IRQHandler, Default_Handler
