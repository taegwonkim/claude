/**
  ******************************************************************************
  * @file    lan8770_example.c
  * @brief   Minimal usage example for the LAN8770 driver skeleton on STM32H7.
  *          Not wired into any build system -- copy the calls below into
  *          your application (e.g. after MX_ETH_Init() in main.c).
  ******************************************************************************
  */

#include "lan8770.h"
#include "lan8770_stm32_port.h"

static lan8770_Object_t LAN8770;

/**
  * @brief  Register the STM32H7 HAL glue as this driver's bus IO, reset the
  *         PHY, and force it into the given BASE-T1 role.
  * @param  role LAN8770_MODE_MASTER or LAN8770_MODE_SLAVE. Exactly one side
  *              of a 100BASE-T1 point-to-point link must be Master and the
  *              other Slave -- pick per your board's role in the harness
  *              (e.g. head unit = Master, camera/sensor = Slave).
  */
int32_t LAN8770_Example_Init(lan8770_MasterSlaveTypeDef role)
{
  lan8770_IOCtx_t IOCtx;

  IOCtx.Init     = LAN8770_PortInit;
  IOCtx.DeInit   = LAN8770_PortDeInit;
  IOCtx.ReadReg  = LAN8770_PortReadReg;
  IOCtx.WriteReg = LAN8770_PortWriteReg;
  IOCtx.GetTick  = LAN8770_PortGetTick;

  LAN8770.DevAddr = LAN8770_PHY_ADDRESS;

  if (LAN8770_RegisterBusIO(&LAN8770, &IOCtx) != LAN8770_STATUS_OK)
  {
    return LAN8770_STATUS_ERROR;
  }

  return LAN8770_Init(&LAN8770, role);
}

/**
  * @brief  Poll the link. Call periodically (e.g. every 100ms from the main
  *         loop, or on the PHY's INT pin once that's wired up) and act on
  *         LAN8770_STATUS_LINK_UP_100M_FD / LAN8770_STATUS_LINK_DOWN, e.g.
  *         to start/stop LwIP's netif.
  */
int32_t LAN8770_Example_PollLink(void)
{
  return LAN8770_GetLinkState(&LAN8770);
}
