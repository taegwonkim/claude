/**
 * Startup file for STM32L552RCTx (Cortex-M33, 256KB Flash, 256KB RAM)
 * Adapted from STM32CubeL5 template.
 */

  .syntax unified
  .cpu cortex-m33
  .fpu softvfp
  .thumb

.global g_pfnVectors
.global Default_Handler

/* start address for the .data section (defined in linker script) */
.word _sidata
.word _sdata
.word _edata
.word _sbss
.word _ebss

  .section .text.Reset_Handler
  .weak Reset_Handler
  .type Reset_Handler, %function
Reset_Handler:
  ldr   r0, =_estack
  mov   sp, r0

  /* Copy .data from FLASH to RAM */
  ldr r0, =_sdata
  ldr r1, =_edata
  ldr r2, =_sidata
  movs r3, #0
  b     LoopCopyDataInit

CopyDataInit:
  ldr r4, [r2, r3]
  str r4, [r0, r3]
  adds r3, r3, #4

LoopCopyDataInit:
  adds r4, r0, r3
  cmp r4, r1
  bcc CopyDataInit

  /* Zero-fill .bss */
  ldr r2, =_sbss
  ldr r4, =_ebss
  movs r3, #0
  b LoopFillZerobss

FillZerobss:
  str r3, [r2]
  adds r2, r2, #4

LoopFillZerobss:
  cmp r2, r4
  bcc FillZerobss

  /* Call static constructors */
  bl __libc_init_array

  /* main */
  bl main
  bx lr

  .size Reset_Handler, .-Reset_Handler

  .section .text.Default_Handler,"ax",%progbits
Default_Handler:
Infinite_Loop:
  b Infinite_Loop
  .size Default_Handler, .-Default_Handler

/* Vector table */
  .section .isr_vector,"a",%progbits
  .type g_pfnVectors, %object

g_pfnVectors:
  .word _estack
  .word Reset_Handler
  .word NMI_Handler
  .word HardFault_Handler
  .word MemManage_Handler
  .word BusFault_Handler
  .word UsageFault_Handler
  .word SecureFault_Handler
  .word 0
  .word 0
  .word 0
  .word SVC_Handler
  .word DebugMon_Handler
  .word 0
  .word PendSV_Handler
  .word SysTick_Handler
  /* External interrupts — add handlers as needed */
  .rept 116
  .word Default_Handler
  .endr

  .size g_pfnVectors, .-g_pfnVectors

/* Weak aliases — override in application code if needed */
  .macro  WEAK_IRQ name
  .weak   \name
  .set    \name, Default_Handler
  .endm

  WEAK_IRQ NMI_Handler
  WEAK_IRQ HardFault_Handler
  WEAK_IRQ MemManage_Handler
  WEAK_IRQ BusFault_Handler
  WEAK_IRQ UsageFault_Handler
  WEAK_IRQ SecureFault_Handler
  WEAK_IRQ SVC_Handler
  WEAK_IRQ DebugMon_Handler
  WEAK_IRQ PendSV_Handler
  WEAK_IRQ SysTick_Handler
