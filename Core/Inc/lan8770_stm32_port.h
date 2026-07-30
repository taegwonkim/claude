/**
  ******************************************************************************
  * @file    lan8770_stm32_port.h
  * @brief   STM32H7 HAL glue for the LAN8770 driver skeleton (Drivers/BSP/
  *          Components/lan8770). Wires lan8770_IOCtx_t to HAL_ETH_xxxPHYRegister().
  *
  *          Assumes a CubeMX-generated project where:
  *            - ETH_HandleTypeDef heth is initialized elsewhere (MX_ETH_Init)
  *              with the RMII interface selected (SYSCFG PMCR ETH_SEL_PHY),
  *            - HAL_ETH_MspInit() / GPIO / clocks for the RMII pins
  *              (REF_CLK, MDIO, MDC, RXD0/1, TXD0/1, TX_EN, CRS_DV) are
  *              already configured by CubeMX for your board.
  *          This file only owns the PHY-facing SMI read/write plumbing.
  ******************************************************************************
  */

#ifndef LAN8770_STM32_PORT_H
#define LAN8770_STM32_PORT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32h7xx_hal.h"
#include "lan8770.h"

/* TODO: confirm the LAN8770's SMI/MDIO address against your board's strap
 * pin configuration (LAN8770 supports address selection via strap
 * resistors) -- 0x00 is only a placeholder used throughout this skeleton. */
#define LAN8770_PHY_ADDRESS   0x00U

/* Provided by the application (typically the CubeMX-generated main.c /
 * ethernetif.c) -- this port layer only reads/writes PHY registers through
 * the already-initialized handle, it does not call MX_ETH_Init() itself. */
extern ETH_HandleTypeDef heth;

int32_t LAN8770_PortInit(void);
int32_t LAN8770_PortDeInit(void);
int32_t LAN8770_PortReadReg(uint32_t DevAddr, uint32_t RegAddr, uint32_t *pRegVal);
int32_t LAN8770_PortWriteReg(uint32_t DevAddr, uint32_t RegAddr, uint32_t RegVal);
int32_t LAN8770_PortGetTick(void);

#ifdef __cplusplus
}
#endif

#endif /* LAN8770_STM32_PORT_H */
