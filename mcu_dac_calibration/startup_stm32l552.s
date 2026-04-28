/* Minimal startup for STM32L552 (Cortex-M33, Thumb-2) */
    .syntax unified
    .cpu cortex-m33
    .thumb

    .section .vectors, "a"
    .align 2
    .global __isr_vector
__isr_vector:
    .word  _estack                  /* 0: Stack top  */
    .word  Reset_Handler            /* 1: Reset      */
    .word  NMI_Handler              /* 2  */
    .word  HardFault_Handler        /* 3  */
    .word  MemManage_Handler        /* 4  */
    .word  BusFault_Handler         /* 5  */
    .word  UsageFault_Handler       /* 6  */
    .word  SecureFault_Handler      /* 7  */
    .word  0                        /* 8  */
    .word  0                        /* 9  */
    .word  0                        /* 10 */
    .word  SVC_Handler              /* 11 */
    .word  DebugMon_Handler         /* 12 */
    .word  0                        /* 13 */
    .word  PendSV_Handler           /* 14 */
    .word  SysTick_Handler          /* 15 */
    /* IRQ 0-15 (extend as needed) */
    .rept  16
    .word  Default_Handler
    .endr

    .section .text.Reset_Handler, "ax"
    .weak  Reset_Handler
    .type  Reset_Handler, %function
Reset_Handler:
    /* Initialise .data from flash */
    ldr  r0, =_sdata
    ldr  r1, =_edata
    ldr  r2, =_sidata
    movs r3, #0
    b    LoopCopyDataInit
CopyDataInit:
    ldr  r4, [r2, r3]
    str  r4, [r0, r3]
    adds r3, r3, #4
LoopCopyDataInit:
    adds r4, r0, r3
    cmp  r4, r1
    bcc  CopyDataInit

    /* Zero-fill .bss */
    ldr  r2, =_sbss
    ldr  r4, =_ebss
    movs r3, #0
    b    LoopFillZero
FillZero:
    str  r3, [r2]
    adds r2, r2, #4
LoopFillZero:
    cmp  r2, r4
    bcc  FillZero

    bl   main
    b    .

    .weak  NMI_Handler
    .weak  HardFault_Handler
    .weak  MemManage_Handler
    .weak  BusFault_Handler
    .weak  UsageFault_Handler
    .weak  SecureFault_Handler
    .weak  SVC_Handler
    .weak  DebugMon_Handler
    .weak  PendSV_Handler
    .weak  SysTick_Handler

Default_Handler:
    b .
    .size Default_Handler, . - Default_Handler
