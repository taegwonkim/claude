/**
  ******************************************************************************
  * @file    lan8770_freertos_task.h
  * @brief   FreeRTOS (CMSIS-RTOS2) wrapper around the LAN8770 driver skeleton.
  *          Runs PHY init + link polling as its own task and mutex-guards all
  *          register access, since MACMDIOAR (the MAC's MDIO controller
  *          register) is shared hardware -- any other task/module doing PHY
  *          reads/writes concurrently (e.g. a custom LwIP ethernetif) must
  *          go through LAN8770_FreeRTOS_ReadReg/WriteReg below rather than
  *          calling the driver directly.
  ******************************************************************************
  */

#ifndef LAN8770_FREERTOS_TASK_H
#define LAN8770_FREERTOS_TASK_H

#ifdef __cplusplus
extern "C" {
#endif

#include "cmsis_os2.h"
#include "lan8770.h"

/* CMSIS_V2 osThreadAttr_t.stack_size is in bytes. */
#define LAN8770_LINK_TASK_STACK_SIZE   (512U)
#define LAN8770_LINK_TASK_PRIORITY     osPriorityNormal
/* Assumes CubeMX default configTICK_RATE_HZ = 1000 (1 ms/tick), so passing
 * a millisecond count straight to osDelay() is valid. If you change the
 * FreeRTOS tick rate in CubeMX, convert accordingly. */
#define LAN8770_LINK_POLL_PERIOD_MS    100U

/**
  * @brief  Create the mutex and link-poll task. Call once from application
  *         code (e.g. CubeMX-generated freertos.c / MX_FREERTOS_Init(),
  *         or from your own startup task) after MX_ETH_Init() has run.
  * @param  role LAN8770_MODE_MASTER or LAN8770_MODE_SLAVE -- fixed per your
  *              board's role on the 100BASE-T1 point-to-point link.
  */
osStatus_t LAN8770_FreeRTOS_Start(lan8770_MasterSlaveTypeDef role);

/**
  * @brief  Weak hook, called from the link-poll task (thread context, not
  *         ISR) whenever the link state changes. Override in application
  *         code, e.g. to call netif_set_link_up()/netif_set_link_down().
  */
void LAN8770_FreeRTOS_OnLinkChange(int32_t LinkState);

/* Mutex-guarded direct register access for other tasks/modules. */
int32_t LAN8770_FreeRTOS_ReadReg(uint16_t RegAddr, uint32_t *pRegVal);
int32_t LAN8770_FreeRTOS_WriteReg(uint16_t RegAddr, uint16_t RegVal);

#ifdef __cplusplus
}
#endif

#endif /* LAN8770_FREERTOS_TASK_H */
