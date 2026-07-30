/**
  ******************************************************************************
  * @file    lan8770.h
  * @brief   Header for lan8770.c, a bus-IO-agnostic driver skeleton for the
  *          Microchip LAN8770 100BASE-T1 automotive Ethernet PHY.
  *
  *          Structured like ST's BSP PHY components (e.g. lan8742.c/h) so it
  *          plugs into the same STM32Cube ETH HAL / LwIP ethernetif.c pattern:
  *          the application registers Init/DeInit/ReadReg/WriteReg/GetTick
  *          function pointers, and this driver never touches the HAL directly.
  *
  *          SKELETON NOTICE: Only registers/bitfields defined by IEEE 802.3
  *          (Clause 22 base registers, Clause 45 BASE-T1 PMA/PMD control via
  *          MMD indirect access) are populated here, since those are
  *          guaranteed by the 802.3bw-2015 standard. LAN8770 vendor-specific
  *          registers (interrupt source/mask, T1 link/SQI status, TC10
  *          sleep/wake, cable diagnostics) are left as TODO placeholders and
  *          MUST be filled in from the official Microchip LAN8770 datasheet
  *          (DS00002580) before this is used on real hardware.
  ******************************************************************************
  */

#ifndef LAN8770_H
#define LAN8770_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* ---------------------------------------------------------------------- */
/* IEEE 802.3 Clause 22 base registers (common to every MII/RMII SMI PHY) */
/* ---------------------------------------------------------------------- */
#define LAN8770_BCR                        ((uint16_t)0x0000U) /*!< Basic Control Register        */
#define LAN8770_BSR                        ((uint16_t)0x0001U) /*!< Basic Status Register         */
#define LAN8770_PHYI1R                      ((uint16_t)0x0002U) /*!< PHY Identifier 1              */
#define LAN8770_PHYI2R                      ((uint16_t)0x0003U) /*!< PHY Identifier 2              */
#define LAN8770_MMDACR                      ((uint16_t)0x000DU) /*!< MMD Access Control Register   */
#define LAN8770_MMDAADR                     ((uint16_t)0x000EU) /*!< MMD Access Address/Data Reg.  */

/* LAN8770 PHY identifier (Microchip OUI-derived value, MSB in PHYI1R).
 * TODO: cross-check exact PHYI1R/PHYI2R reset values against the datasheet;
 * this is the value expected from the part's SMI ID registers. */
#define LAN8770_PHY_ID1                     ((uint32_t)0x0007U)
#define LAN8770_PHY_ID2_MODEL_MASK          ((uint32_t)0xFFF0U) /*!< mask out revision bits */

/* BCR (0x00) bit definitions */
#define LAN8770_BCR_SOFT_RESET              ((uint16_t)0x8000U)
#define LAN8770_BCR_LOOPBACK                ((uint16_t)0x4000U)
#define LAN8770_BCR_POWER_DOWN              ((uint16_t)0x0800U)
#define LAN8770_BCR_ISOLATE                 ((uint16_t)0x0400U)

/* BSR (0x01) bit definitions */
#define LAN8770_BSR_LINK_STATUS             ((uint16_t)0x0004U)
#define LAN8770_BSR_REMOTE_FAULT            ((uint16_t)0x0010U)

/* MMD Access Control Register (0x0D) function field */
#define LAN8770_MMDACR_FUNC_ADDRESS         ((uint16_t)0x0000U) /*!< next MMDAADR write = address */
#define LAN8770_MMDACR_FUNC_DATA_NO_INCR    ((uint16_t)0x4000U) /*!< next MMDAADR access = data    */
#define LAN8770_MMDACR_DEVAD_MASK           ((uint16_t)0x001FU)

/* Clause 45 MMD device addresses used to reach BASE-T1 PMA/PMD registers */
#define LAN8770_MMD_PMAPMD                  ((uint16_t)1U)  /*!< IEEE MDIO_MMD_PMAPMD */

/* IEEE 802.3bw-2015 BASE-T1 PMA/PMD control register, reached through the
 * MMD indirect-access mechanism above (Clause 22.2.4.3).
 * Address 0x0834 (2100 decimal) == MDIO_PMA_PMD_BT1_CTRL in the Linux
 * kernel's include/uapi/linux/mdio.h -- verified IEEE-standard value, not
 * vendor-specific, so it applies to any compliant 100BASE-T1 PHY. */
#define LAN8770_BT1_CTRL_REG                ((uint16_t)0x0834U)
#define LAN8770_BT1_CTRL_CFG_MST            ((uint16_t)0x4000U) /*!< 1 = Master, 0 = Slave */

/* -------------------------------------------------------------------------
 * TODO (datasheet-dependent, LAN8770 vendor-specific -- NOT filled in):
 *   - T1 link-up / SQI / cable-diagnostics status registers
 *   - Interrupt source register (INT_SRC) / interrupt mask register (INT_MASK)
 *   - TC10 sleep/wake control register
 *   - Strap-configured default PHY (MDIO) address
 * Confirm exact addresses/bitfields against Microchip DS00002580 (LAN8770
 * datasheet) and DS50002978A (EVB-LAN8770-RMII user guide) before use.
 * ---------------------------------------------------------------------- */

/* ---------------------------------------------------------------------- */
/* Bus IO context -- mirrors the ST BSP PHY component pattern (lan8742,    */
/* dp83848, ...) so this driver is transport-agnostic: the STM32H7 HAL    */
/* glue (see lan8770_stm32_port.c) is the only place that calls into      */
/* HAL_ETH_ReadPHYRegister()/HAL_ETH_WritePHYRegister().                  */
/* ---------------------------------------------------------------------- */
typedef int32_t (*lan8770_Init_Func)(void);
typedef int32_t (*lan8770_DeInit_Func)(void);
typedef int32_t (*lan8770_WriteReg_Func)(uint32_t DevAddr, uint32_t RegAddr, uint32_t RegVal);
typedef int32_t (*lan8770_ReadReg_Func)(uint32_t DevAddr, uint32_t RegAddr, uint32_t *pRegVal);
typedef int32_t (*lan8770_GetTick_Func)(void);

typedef struct
{
  lan8770_Init_Func     Init;
  lan8770_DeInit_Func   DeInit;
  lan8770_WriteReg_Func WriteReg;
  lan8770_ReadReg_Func  ReadReg;
  lan8770_GetTick_Func  GetTick;
} lan8770_IOCtx_t;

typedef enum
{
  LAN8770_STATUS_READ_ERROR          = -6,
  LAN8770_STATUS_WRITE_ERROR         = -5,
  LAN8770_STATUS_ADDRESS_ERROR       = -4,
  LAN8770_STATUS_RESET_TIMEOUT       = -3,
  LAN8770_STATUS_NOT_INITIALIZED     = -2,
  LAN8770_STATUS_ERROR               = -1,
  LAN8770_STATUS_OK                  =  0,
  LAN8770_STATUS_LINK_DOWN           =  1,
  /* 100BASE-T1 is a fixed-rate, always full-duplex, point-to-point link:
   * there is no Clause 28 autonegotiation of speed/duplex like classic
   * 100BASE-TX, so "link up" only ever means this one state. */
  LAN8770_STATUS_LINK_UP_100M_FD     =  2,
} lan8770_StatusTypeDef;

typedef enum
{
  LAN8770_MODE_SLAVE  = 0,
  LAN8770_MODE_MASTER = 1,
} lan8770_MasterSlaveTypeDef;

typedef struct
{
  uint32_t                    DevAddr;
  uint32_t                    Is_Initialized;
  lan8770_IOCtx_t              IO;
  void                        *pData;
  lan8770_MasterSlaveTypeDef   MasterSlaveMode;
  int32_t                      LinkStatus;
} lan8770_Object_t;

/* Registration / lifecycle */
int32_t LAN8770_RegisterBusIO(lan8770_Object_t *pObj, lan8770_IOCtx_t *pIO);
int32_t LAN8770_Init(lan8770_Object_t *pObj, lan8770_MasterSlaveTypeDef MasterSlave);
int32_t LAN8770_DeInit(lan8770_Object_t *pObj);
int32_t LAN8770_Reset(lan8770_Object_t *pObj);

/* Link / mode control */
int32_t LAN8770_GetLinkState(lan8770_Object_t *pObj);
int32_t LAN8770_SetMasterSlaveMode(lan8770_Object_t *pObj, lan8770_MasterSlaveTypeDef Mode);

/* Test aids */
int32_t LAN8770_EnableLoopbackMode(lan8770_Object_t *pObj);
int32_t LAN8770_DisableLoopbackMode(lan8770_Object_t *pObj);

/* Raw register access, exposed for the vendor-specific registers that are
 * still TODO above -- callers can use these directly once addresses are
 * confirmed from the datasheet. */
int32_t LAN8770_ReadReg(lan8770_Object_t *pObj, uint16_t RegAddr, uint32_t *pRegVal);
int32_t LAN8770_WriteReg(lan8770_Object_t *pObj, uint16_t RegAddr, uint16_t RegVal);
int32_t LAN8770_ReadMMDReg(lan8770_Object_t *pObj, uint16_t MMDDevAddr, uint16_t RegAddr, uint32_t *pRegVal);
int32_t LAN8770_WriteMMDReg(lan8770_Object_t *pObj, uint16_t MMDDevAddr, uint16_t RegAddr, uint16_t RegVal);

#ifdef __cplusplus
}
#endif

#endif /* LAN8770_H */
