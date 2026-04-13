/**
 ******************************************************************************
 * @file    startup_stm32l552retx.s
 * @brief   STM32L552RET6 Cortex-M33 시작 코드 (GCC 어셈블리)
 *
 * 리셋 시 동작:
 *   1. 스택 포인터(MSP) 초기화
 *   2. SystemInit() 호출
 *   3. .data 섹션을 Flash에서 RAM으로 복사
 *   4. .bss 섹션을 0으로 초기화
 *   5. __libc_init_array() 호출 (C++ 생성자 등)
 *   6. main() 호출
 ******************************************************************************
 */

  .syntax unified
  .cpu cortex-m33
  .fpu softvfp
  .thumb

.global g_pfnVectors
.global Default_Handler

/* Linker script symbols */
.word _sidata    /* .data init values in flash */
.word _sdata     /* .data start in RAM */
.word _edata     /* .data end in RAM */
.word _sbss      /* .bss start */
.word _ebss      /* .bss end */

/**
 * @brief  Reset Handler
 */
  .section .text.Reset_Handler
  .weak Reset_Handler
  .type Reset_Handler, %function
Reset_Handler:
  ldr   sp, =_estack    /* Set stack pointer */

  /* Call SystemInit */
  bl    SystemInit

  /* Copy .data section from Flash to RAM */
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

  /* Zero fill .bss section */
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

  /* Call static constructors */
  bl    __libc_init_array

  /* Call main */
  bl    main

  /* Loop forever if main returns */
  b     .

  .size Reset_Handler, .-Reset_Handler

/**
 * @brief  Default Handler for unhandled interrupts
 */
  .section .text.Default_Handler,"ax",%progbits
Default_Handler:
Infinite_Loop:
  b     Infinite_Loop
  .size Default_Handler, .-Default_Handler

/**
 * @brief  Interrupt Vector Table
 */
  .section .isr_vector,"a",%progbits
  .type g_pfnVectors, %object
  .size g_pfnVectors, .-g_pfnVectors

g_pfnVectors:
  /* Cortex-M33 Core Handlers */
  .word _estack                   /* Top of Stack */
  .word Reset_Handler             /* Reset Handler */
  .word NMI_Handler               /* NMI Handler */
  .word HardFault_Handler         /* Hard Fault Handler */
  .word MemManage_Handler         /* MPU Fault Handler */
  .word BusFault_Handler          /* Bus Fault Handler */
  .word UsageFault_Handler        /* Usage Fault Handler */
  .word SecureFault_Handler       /* Secure Fault Handler */
  .word 0                         /* Reserved */
  .word 0                         /* Reserved */
  .word 0                         /* Reserved */
  .word SVC_Handler               /* SVCall Handler */
  .word DebugMon_Handler          /* Debug Monitor Handler */
  .word 0                         /* Reserved */
  .word PendSV_Handler            /* PendSV Handler */
  .word SysTick_Handler           /* SysTick Handler */

  /* STM32L552 External Interrupts */
  .word WWDG_IRQHandler                 /* 0: Window Watchdog */
  .word PVD_PVM_IRQHandler              /* 1: PVD/PVM */
  .word RTC_IRQHandler                  /* 2: RTC */
  .word RTC_S_IRQHandler                /* 3: RTC Secure */
  .word TAMP_IRQHandler                 /* 4: Tamper */
  .word TAMP_S_IRQHandler               /* 5: Tamper Secure */
  .word FLASH_IRQHandler                /* 6: Flash */
  .word FLASH_S_IRQHandler              /* 7: Flash Secure */
  .word GTZC_IRQHandler                 /* 8: GTZC */
  .word RCC_IRQHandler                  /* 9: RCC */
  .word RCC_S_IRQHandler                /* 10: RCC Secure */
  .word EXTI0_IRQHandler                /* 11: EXTI Line 0 */
  .word EXTI1_IRQHandler                /* 12: EXTI Line 1 */
  .word EXTI2_IRQHandler                /* 13: EXTI Line 2 */
  .word EXTI3_IRQHandler                /* 14: EXTI Line 3 */
  .word EXTI4_IRQHandler                /* 15: EXTI Line 4 */
  .word EXTI5_IRQHandler                /* 16: EXTI Line 5 */
  .word EXTI6_IRQHandler                /* 17: EXTI Line 6 */
  .word EXTI7_IRQHandler                /* 18: EXTI Line 7 */
  .word EXTI8_IRQHandler                /* 19: EXTI Line 8 */
  .word EXTI9_IRQHandler                /* 20: EXTI Line 9 */
  .word EXTI10_IRQHandler               /* 21: EXTI Line 10 */
  .word EXTI11_IRQHandler               /* 22: EXTI Line 11 */
  .word EXTI12_IRQHandler               /* 23: EXTI Line 12 */
  .word EXTI13_IRQHandler               /* 24: EXTI Line 13 */
  .word EXTI14_IRQHandler               /* 25: EXTI Line 14 */
  .word EXTI15_IRQHandler               /* 26: EXTI Line 15 */
  .word DMAMUX1_IRQHandler              /* 27: DMAMUX1 */
  .word DMAMUX1_S_IRQHandler            /* 28: DMAMUX1 Secure */
  .word DMA1_Channel1_IRQHandler        /* 29: DMA1 CH1 */
  .word DMA1_Channel2_IRQHandler        /* 30: DMA1 CH2 */
  .word DMA1_Channel3_IRQHandler        /* 31: DMA1 CH3 */
  .word DMA1_Channel4_IRQHandler        /* 32: DMA1 CH4 */
  .word DMA1_Channel5_IRQHandler        /* 33: DMA1 CH5 */
  .word DMA1_Channel6_IRQHandler        /* 34: DMA1 CH6 */
  .word DMA1_Channel7_IRQHandler        /* 35: DMA1 CH7 */
  .word DMA1_Channel8_IRQHandler        /* 36: DMA1 CH8 */
  .word ADC1_2_IRQHandler               /* 37: ADC1_2 */
  .word DAC_IRQHandler                  /* 38: DAC */
  .word FDCAN1_IT0_IRQHandler           /* 39: FDCAN1 IT0 */
  .word FDCAN1_IT1_IRQHandler           /* 40: FDCAN1 IT1 */
  .word TIM1_BRK_IRQHandler             /* 41: TIM1 Break */
  .word TIM1_UP_IRQHandler              /* 42: TIM1 Update */
  .word TIM1_TRG_COM_IRQHandler         /* 43: TIM1 Trigger/COM */
  .word TIM1_CC_IRQHandler              /* 44: TIM1 Capture/Compare */
  .word TIM2_IRQHandler                 /* 45: TIM2 */
  .word TIM3_IRQHandler                 /* 46: TIM3 */
  .word TIM4_IRQHandler                 /* 47: TIM4 */
  .word TIM5_IRQHandler                 /* 48: TIM5 */
  .word TIM6_IRQHandler                 /* 49: TIM6 */
  .word TIM7_IRQHandler                 /* 50: TIM7 */
  .word TIM8_BRK_IRQHandler             /* 51: TIM8 Break */
  .word TIM8_UP_IRQHandler              /* 52: TIM8 Update */
  .word TIM8_TRG_COM_IRQHandler         /* 53: TIM8 Trigger/COM */
  .word TIM8_CC_IRQHandler              /* 54: TIM8 Capture/Compare */
  .word I2C1_EV_IRQHandler              /* 55: I2C1 Event */
  .word I2C1_ER_IRQHandler              /* 56: I2C1 Error */
  .word I2C2_EV_IRQHandler              /* 57: I2C2 Event */
  .word I2C2_ER_IRQHandler              /* 58: I2C2 Error */
  .word SPI1_IRQHandler                 /* 59: SPI1 */
  .word SPI2_IRQHandler                 /* 60: SPI2 */
  .word USART1_IRQHandler               /* 61: USART1 */
  .word USART2_IRQHandler               /* 62: USART2 */
  .word USART3_IRQHandler               /* 63: USART3 */

  /* Weak aliases for all handlers → Default_Handler */
  .weak NMI_Handler
  .thumb_set NMI_Handler, Default_Handler
  .weak HardFault_Handler
  .thumb_set HardFault_Handler, Default_Handler
  .weak MemManage_Handler
  .thumb_set MemManage_Handler, Default_Handler
  .weak BusFault_Handler
  .thumb_set BusFault_Handler, Default_Handler
  .weak UsageFault_Handler
  .thumb_set UsageFault_Handler, Default_Handler
  .weak SecureFault_Handler
  .thumb_set SecureFault_Handler, Default_Handler
  .weak SVC_Handler
  .thumb_set SVC_Handler, Default_Handler
  .weak DebugMon_Handler
  .thumb_set DebugMon_Handler, Default_Handler
  .weak PendSV_Handler
  .thumb_set PendSV_Handler, Default_Handler
  .weak SysTick_Handler
  .thumb_set SysTick_Handler, Default_Handler

  .weak WWDG_IRQHandler
  .thumb_set WWDG_IRQHandler, Default_Handler
  .weak PVD_PVM_IRQHandler
  .thumb_set PVD_PVM_IRQHandler, Default_Handler
  .weak RTC_IRQHandler
  .thumb_set RTC_IRQHandler, Default_Handler
  .weak RTC_S_IRQHandler
  .thumb_set RTC_S_IRQHandler, Default_Handler
  .weak TAMP_IRQHandler
  .thumb_set TAMP_IRQHandler, Default_Handler
  .weak TAMP_S_IRQHandler
  .thumb_set TAMP_S_IRQHandler, Default_Handler
  .weak FLASH_IRQHandler
  .thumb_set FLASH_IRQHandler, Default_Handler
  .weak FLASH_S_IRQHandler
  .thumb_set FLASH_S_IRQHandler, Default_Handler
  .weak GTZC_IRQHandler
  .thumb_set GTZC_IRQHandler, Default_Handler
  .weak RCC_IRQHandler
  .thumb_set RCC_IRQHandler, Default_Handler
  .weak RCC_S_IRQHandler
  .thumb_set RCC_S_IRQHandler, Default_Handler
  .weak EXTI0_IRQHandler
  .thumb_set EXTI0_IRQHandler, Default_Handler
  .weak EXTI1_IRQHandler
  .thumb_set EXTI1_IRQHandler, Default_Handler
  .weak EXTI2_IRQHandler
  .thumb_set EXTI2_IRQHandler, Default_Handler
  .weak EXTI3_IRQHandler
  .thumb_set EXTI3_IRQHandler, Default_Handler
  .weak EXTI4_IRQHandler
  .thumb_set EXTI4_IRQHandler, Default_Handler
  .weak EXTI5_IRQHandler
  .thumb_set EXTI5_IRQHandler, Default_Handler
  .weak EXTI6_IRQHandler
  .thumb_set EXTI6_IRQHandler, Default_Handler
  .weak EXTI7_IRQHandler
  .thumb_set EXTI7_IRQHandler, Default_Handler
  .weak EXTI8_IRQHandler
  .thumb_set EXTI8_IRQHandler, Default_Handler
  .weak EXTI9_IRQHandler
  .thumb_set EXTI9_IRQHandler, Default_Handler
  .weak EXTI10_IRQHandler
  .thumb_set EXTI10_IRQHandler, Default_Handler
  .weak EXTI11_IRQHandler
  .thumb_set EXTI11_IRQHandler, Default_Handler
  .weak EXTI12_IRQHandler
  .thumb_set EXTI12_IRQHandler, Default_Handler
  .weak EXTI13_IRQHandler
  .thumb_set EXTI13_IRQHandler, Default_Handler
  .weak EXTI14_IRQHandler
  .thumb_set EXTI14_IRQHandler, Default_Handler
  .weak EXTI15_IRQHandler
  .thumb_set EXTI15_IRQHandler, Default_Handler
  .weak DMAMUX1_IRQHandler
  .thumb_set DMAMUX1_IRQHandler, Default_Handler
  .weak DMAMUX1_S_IRQHandler
  .thumb_set DMAMUX1_S_IRQHandler, Default_Handler
  .weak DMA1_Channel1_IRQHandler
  .thumb_set DMA1_Channel1_IRQHandler, Default_Handler
  .weak DMA1_Channel2_IRQHandler
  .thumb_set DMA1_Channel2_IRQHandler, Default_Handler
  .weak DMA1_Channel3_IRQHandler
  .thumb_set DMA1_Channel3_IRQHandler, Default_Handler
  .weak DMA1_Channel4_IRQHandler
  .thumb_set DMA1_Channel4_IRQHandler, Default_Handler
  .weak DMA1_Channel5_IRQHandler
  .thumb_set DMA1_Channel5_IRQHandler, Default_Handler
  .weak DMA1_Channel6_IRQHandler
  .thumb_set DMA1_Channel6_IRQHandler, Default_Handler
  .weak DMA1_Channel7_IRQHandler
  .thumb_set DMA1_Channel7_IRQHandler, Default_Handler
  .weak DMA1_Channel8_IRQHandler
  .thumb_set DMA1_Channel8_IRQHandler, Default_Handler
  .weak ADC1_2_IRQHandler
  .thumb_set ADC1_2_IRQHandler, Default_Handler
  .weak DAC_IRQHandler
  .thumb_set DAC_IRQHandler, Default_Handler
  .weak FDCAN1_IT0_IRQHandler
  .thumb_set FDCAN1_IT0_IRQHandler, Default_Handler
  .weak FDCAN1_IT1_IRQHandler
  .thumb_set FDCAN1_IT1_IRQHandler, Default_Handler
  .weak TIM1_BRK_IRQHandler
  .thumb_set TIM1_BRK_IRQHandler, Default_Handler
  .weak TIM1_UP_IRQHandler
  .thumb_set TIM1_UP_IRQHandler, Default_Handler
  .weak TIM1_TRG_COM_IRQHandler
  .thumb_set TIM1_TRG_COM_IRQHandler, Default_Handler
  .weak TIM1_CC_IRQHandler
  .thumb_set TIM1_CC_IRQHandler, Default_Handler
  .weak TIM2_IRQHandler
  .thumb_set TIM2_IRQHandler, Default_Handler
  .weak TIM3_IRQHandler
  .thumb_set TIM3_IRQHandler, Default_Handler
  .weak TIM4_IRQHandler
  .thumb_set TIM4_IRQHandler, Default_Handler
  .weak TIM5_IRQHandler
  .thumb_set TIM5_IRQHandler, Default_Handler
  .weak TIM6_IRQHandler
  .thumb_set TIM6_IRQHandler, Default_Handler
  .weak TIM7_IRQHandler
  .thumb_set TIM7_IRQHandler, Default_Handler
  .weak TIM8_BRK_IRQHandler
  .thumb_set TIM8_BRK_IRQHandler, Default_Handler
  .weak TIM8_UP_IRQHandler
  .thumb_set TIM8_UP_IRQHandler, Default_Handler
  .weak TIM8_TRG_COM_IRQHandler
  .thumb_set TIM8_TRG_COM_IRQHandler, Default_Handler
  .weak TIM8_CC_IRQHandler
  .thumb_set TIM8_CC_IRQHandler, Default_Handler
  .weak I2C1_EV_IRQHandler
  .thumb_set I2C1_EV_IRQHandler, Default_Handler
  .weak I2C1_ER_IRQHandler
  .thumb_set I2C1_ER_IRQHandler, Default_Handler
  .weak I2C2_EV_IRQHandler
  .thumb_set I2C2_EV_IRQHandler, Default_Handler
  .weak I2C2_ER_IRQHandler
  .thumb_set I2C2_ER_IRQHandler, Default_Handler
  .weak SPI1_IRQHandler
  .thumb_set SPI1_IRQHandler, Default_Handler
  .weak SPI2_IRQHandler
  .thumb_set SPI2_IRQHandler, Default_Handler
  .weak USART1_IRQHandler
  .thumb_set USART1_IRQHandler, Default_Handler
  .weak USART2_IRQHandler
  .thumb_set USART2_IRQHandler, Default_Handler
  .weak USART3_IRQHandler
  .thumb_set USART3_IRQHandler, Default_Handler
