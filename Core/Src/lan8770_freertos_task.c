/**
  ******************************************************************************
  * @file    lan8770_freertos_task.c
  * @brief   FreeRTOS (CMSIS-RTOS2) wrapper around the LAN8770 driver skeleton.
  *          See lan8770_freertos_task.h for the threading/locking contract.
  ******************************************************************************
  */

#include "lan8770_freertos_task.h"
#include "lan8770_stm32_port.h"

static lan8770_Object_t     LAN8770;
static osMutexId_t          LAN8770_MutexHandle;
static osThreadId_t         LAN8770_LinkTaskHandle;
static lan8770_MasterSlaveTypeDef LAN8770_StartupRole;

static const osMutexAttr_t LAN8770_Mutex_Attr =
{
  .name = "LAN8770Mutex",
};

static const osThreadAttr_t LAN8770_LinkTask_Attr =
{
  .name       = "LAN8770LinkTask",
  .stack_size = LAN8770_LINK_TASK_STACK_SIZE,
  .priority   = (osPriority_t)LAN8770_LINK_TASK_PRIORITY,
};

static void LAN8770_LinkTask(void *argument);

__weak void LAN8770_FreeRTOS_OnLinkChange(int32_t LinkState)
{
  (void)LinkState;
  /* Default no-op. Example override, e.g. in your LwIP glue:
   *
   *   void LAN8770_FreeRTOS_OnLinkChange(int32_t LinkState)
   *   {
   *     if (LinkState == LAN8770_STATUS_LINK_UP_100M_FD)
   *     {
   *       netif_set_link_up(&gnetif);
   *     }
   *     else
   *     {
   *       netif_set_link_down(&gnetif);
   *     }
   *   }
   */
}

osStatus_t LAN8770_FreeRTOS_Start(lan8770_MasterSlaveTypeDef role)
{
  lan8770_IOCtx_t IOCtx;

  LAN8770_MutexHandle = osMutexNew(&LAN8770_Mutex_Attr);
  if (LAN8770_MutexHandle == NULL)
  {
    return osError;
  }

  IOCtx.Init     = LAN8770_PortInit;
  IOCtx.DeInit   = LAN8770_PortDeInit;
  IOCtx.ReadReg  = LAN8770_PortReadReg;
  IOCtx.WriteReg = LAN8770_PortWriteReg;
  IOCtx.GetTick  = LAN8770_PortGetTick;

  LAN8770.DevAddr = LAN8770_PHY_ADDRESS;

  if (LAN8770_RegisterBusIO(&LAN8770, &IOCtx) != LAN8770_STATUS_OK)
  {
    return osError;
  }

  /* PHY reset/init (can legitimately take up to LAN8770_Reset()'s ~500 ms
   * timeout) runs inside the task itself, not here, so a slow PHY never
   * blocks whichever caller/task invokes LAN8770_FreeRTOS_Start() during
   * system startup. */
  LAN8770_StartupRole = role;
  LAN8770_LinkTaskHandle = osThreadNew(LAN8770_LinkTask, NULL, &LAN8770_LinkTask_Attr);
  if (LAN8770_LinkTaskHandle == NULL)
  {
    return osError;
  }

  return osOK;
}

static void LAN8770_LinkTask(void *argument)
{
  int32_t previousState = LAN8770_STATUS_LINK_DOWN;
  int32_t currentState;
  int32_t initStatus;

  (void)argument;

  osMutexAcquire(LAN8770_MutexHandle, osWaitForever);
  initStatus = LAN8770_Init(&LAN8770, LAN8770_StartupRole);
  osMutexRelease(LAN8770_MutexHandle);

  if (initStatus != LAN8770_STATUS_OK)
  {
    /* TODO: surface this to the application (event flag, log, etc.) instead
     * of silently retrying link polls against an unresponsive PHY below --
     * left as-is here since fault reporting is application-specific. */
  }

  for (;;)
  {
    osMutexAcquire(LAN8770_MutexHandle, osWaitForever);
    currentState = LAN8770_GetLinkState(&LAN8770);
    osMutexRelease(LAN8770_MutexHandle);

    if (currentState != previousState)
    {
      LAN8770_FreeRTOS_OnLinkChange(currentState);
      previousState = currentState;
    }

    osDelay(LAN8770_LINK_POLL_PERIOD_MS);
  }
}

int32_t LAN8770_FreeRTOS_ReadReg(uint16_t RegAddr, uint32_t *pRegVal)
{
  int32_t status;

  osMutexAcquire(LAN8770_MutexHandle, osWaitForever);
  status = LAN8770_ReadReg(&LAN8770, RegAddr, pRegVal);
  osMutexRelease(LAN8770_MutexHandle);

  return status;
}

int32_t LAN8770_FreeRTOS_WriteReg(uint16_t RegAddr, uint16_t RegVal)
{
  int32_t status;

  osMutexAcquire(LAN8770_MutexHandle, osWaitForever);
  status = LAN8770_WriteReg(&LAN8770, RegAddr, RegVal);
  osMutexRelease(LAN8770_MutexHandle);

  return status;
}
