/**
  ******************************************************************************
  * @file    lan8770.c
  * @brief   Driver skeleton for the Microchip LAN8770 100BASE-T1 automotive
  *          Ethernet PHY. See lan8770.h for scope notes / TODO list.
  ******************************************************************************
  */

#include "lan8770.h"

#define LAN8770_SW_RESET_TIMEOUT   ((uint32_t)500U) /* ms, generous margin */

static int32_t LAN8770_ReadRegWrap(lan8770_Object_t *pObj, uint16_t RegAddr, uint32_t *pRegVal);
static int32_t LAN8770_WriteRegWrap(lan8770_Object_t *pObj, uint16_t RegAddr, uint16_t RegVal);

/**
  * @brief  Register the bus IO context (Init/DeInit/ReadReg/WriteReg/GetTick)
  *         that this driver will use for all SMI/MDIO access.
  */
int32_t LAN8770_RegisterBusIO(lan8770_Object_t *pObj, lan8770_IOCtx_t *pIO)
{
  if ((pObj == NULL) || (pIO == NULL))
  {
    return LAN8770_STATUS_ERROR;
  }

  if ((pIO->Init == NULL) || (pIO->DeInit == NULL) ||
      (pIO->ReadReg == NULL) || (pIO->WriteReg == NULL) || (pIO->GetTick == NULL))
  {
    return LAN8770_STATUS_ERROR;
  }

  pObj->IO.Init     = pIO->Init;
  pObj->IO.DeInit   = pIO->DeInit;
  pObj->IO.ReadReg  = pIO->ReadReg;
  pObj->IO.WriteReg = pIO->WriteReg;
  pObj->IO.GetTick  = pIO->GetTick;

  pObj->Is_Initialized = 0;
  pObj->LinkStatus     = 0;

  return LAN8770_STATUS_OK;
}

/**
  * @brief  Bring the PHY out of reset, verify its ID, and force the
  *         Master/Slave role required for a 100BASE-T1 point-to-point link
  *         (unlike classic Ethernet, there is no autonegotiation of this --
  *         exactly one side of the pair must be Master, the other Slave).
  */
int32_t LAN8770_Init(lan8770_Object_t *pObj, lan8770_MasterSlaveTypeDef MasterSlave)
{
  int32_t status = LAN8770_STATUS_OK;
  uint32_t regval;

  if (pObj == NULL)
  {
    return LAN8770_STATUS_ERROR;
  }

  if (pObj->Is_Initialized != 0U)
  {
    return LAN8770_STATUS_OK;
  }

  if (pObj->IO.Init() != 0)
  {
    return LAN8770_STATUS_ERROR;
  }

  status = LAN8770_Reset(pObj);
  if (status != LAN8770_STATUS_OK)
  {
    return status;
  }

  /* Sanity-check the SMI address actually reaches a LAN8770.
   * TODO: confirm LAN8770_PHY_ID1 / LAN8770_PHY_ID2_MODEL_MASK reset values
   * against the datasheet -- see note in lan8770.h. Left non-fatal (only
   * logged via return value) so the skeleton still runs before that is
   * verified on real hardware. */
  if (LAN8770_ReadRegWrap(pObj, LAN8770_PHYI1R, &regval) != LAN8770_STATUS_OK)
  {
    return LAN8770_STATUS_READ_ERROR;
  }
  if ((regval & 0xFFFFU) != LAN8770_PHY_ID1)
  {
    status = LAN8770_STATUS_ADDRESS_ERROR; /* wrong/absent PHY at DevAddr */
  }

  if (LAN8770_SetMasterSlaveMode(pObj, MasterSlave) != LAN8770_STATUS_OK)
  {
    return LAN8770_STATUS_WRITE_ERROR;
  }

  pObj->Is_Initialized = 1;

  return status;
}

/**
  * @brief  Release the bus IO layer. Does not touch PHY registers.
  */
int32_t LAN8770_DeInit(lan8770_Object_t *pObj)
{
  if (pObj == NULL)
  {
    return LAN8770_STATUS_ERROR;
  }

  if (pObj->Is_Initialized != 0U)
  {
    if (pObj->IO.DeInit() != 0)
    {
      return LAN8770_STATUS_ERROR;
    }
    pObj->Is_Initialized = 0;
  }

  return LAN8770_STATUS_OK;
}

/**
  * @brief  Issue a Clause 22 soft reset (BCR.15) and poll until the PHY
  *         clears the bit itself, per IEEE 802.3.
  */
int32_t LAN8770_Reset(lan8770_Object_t *pObj)
{
  uint32_t regval;
  int32_t  tickstart;

  if (pObj == NULL)
  {
    return LAN8770_STATUS_ERROR;
  }

  if (LAN8770_WriteRegWrap(pObj, LAN8770_BCR, LAN8770_BCR_SOFT_RESET) != LAN8770_STATUS_OK)
  {
    return LAN8770_STATUS_WRITE_ERROR;
  }

  tickstart = pObj->IO.GetTick();

  do
  {
    if (LAN8770_ReadRegWrap(pObj, LAN8770_BCR, &regval) != LAN8770_STATUS_OK)
    {
      return LAN8770_STATUS_READ_ERROR;
    }

    if ((pObj->IO.GetTick() - tickstart) > (int32_t)LAN8770_SW_RESET_TIMEOUT)
    {
      return LAN8770_STATUS_RESET_TIMEOUT;
    }
  } while ((regval & LAN8770_BCR_SOFT_RESET) != 0U);

  return LAN8770_STATUS_OK;
}

/**
  * @brief  Read the (latching-low) link status bit. Per IEEE 802.3 the bit
  *         must be read twice: the first read returns the latched "was down
  *         since last read" value, the second the live value.
  * @retval LAN8770_STATUS_LINK_DOWN, LAN8770_STATUS_LINK_UP_100M_FD, or a
  *         negative lan8770_StatusTypeDef error code.
  */
int32_t LAN8770_GetLinkState(lan8770_Object_t *pObj)
{
  uint32_t bsr;

  if (pObj == NULL)
  {
    return LAN8770_STATUS_ERROR;
  }

  if (LAN8770_ReadRegWrap(pObj, LAN8770_BSR, &bsr) != LAN8770_STATUS_OK)
  {
    return LAN8770_STATUS_READ_ERROR;
  }
  if (LAN8770_ReadRegWrap(pObj, LAN8770_BSR, &bsr) != LAN8770_STATUS_OK)
  {
    return LAN8770_STATUS_READ_ERROR;
  }

  if ((bsr & LAN8770_BSR_LINK_STATUS) == 0U)
  {
    pObj->LinkStatus = LAN8770_STATUS_LINK_DOWN;
  }
  else
  {
    /* 100BASE-T1 has no in-band speed/duplex negotiation to read back --
     * a set link bit means 100 Mb/s full duplex, always. */
    pObj->LinkStatus = LAN8770_STATUS_LINK_UP_100M_FD;
  }

  return pObj->LinkStatus;
}

/**
  * @brief  Force BASE-T1 Master or Slave role via the IEEE-standard
  *         BASE-T1 PMA/PMD control register, reached through Clause 45 MMD
  *         indirect access (Clause 22.2.4.3) since RMII PHYs are usually
  *         only wired for Clause 22 (5-bit) SMI addressing.
  */
int32_t LAN8770_SetMasterSlaveMode(lan8770_Object_t *pObj, lan8770_MasterSlaveTypeDef Mode)
{
  uint16_t regval = (Mode == LAN8770_MODE_MASTER) ? LAN8770_BT1_CTRL_CFG_MST : 0x0000U;

  if (pObj == NULL)
  {
    return LAN8770_STATUS_ERROR;
  }

  if (LAN8770_WriteMMDReg(pObj, LAN8770_MMD_PMAPMD, LAN8770_BT1_CTRL_REG, regval) != LAN8770_STATUS_OK)
  {
    return LAN8770_STATUS_WRITE_ERROR;
  }

  pObj->MasterSlaveMode = Mode;

  return LAN8770_STATUS_OK;
}

int32_t LAN8770_EnableLoopbackMode(lan8770_Object_t *pObj)
{
  uint32_t regval;

  if (pObj == NULL)
  {
    return LAN8770_STATUS_ERROR;
  }
  if (LAN8770_ReadRegWrap(pObj, LAN8770_BCR, &regval) != LAN8770_STATUS_OK)
  {
    return LAN8770_STATUS_READ_ERROR;
  }
  regval |= LAN8770_BCR_LOOPBACK;
  return LAN8770_WriteRegWrap(pObj, LAN8770_BCR, (uint16_t)regval);
}

int32_t LAN8770_DisableLoopbackMode(lan8770_Object_t *pObj)
{
  uint32_t regval;

  if (pObj == NULL)
  {
    return LAN8770_STATUS_ERROR;
  }
  if (LAN8770_ReadRegWrap(pObj, LAN8770_BCR, &regval) != LAN8770_STATUS_OK)
  {
    return LAN8770_STATUS_READ_ERROR;
  }
  regval &= ~((uint32_t)LAN8770_BCR_LOOPBACK);
  return LAN8770_WriteRegWrap(pObj, LAN8770_BCR, (uint16_t)regval);
}

/**
  * @brief  Direct Clause 22 register access, exposed so callers can reach
  *         the still-TODO vendor-specific registers once their addresses
  *         are confirmed from the datasheet.
  */
int32_t LAN8770_ReadReg(lan8770_Object_t *pObj, uint16_t RegAddr, uint32_t *pRegVal)
{
  if ((pObj == NULL) || (pRegVal == NULL))
  {
    return LAN8770_STATUS_ERROR;
  }
  return LAN8770_ReadRegWrap(pObj, RegAddr, pRegVal);
}

int32_t LAN8770_WriteReg(lan8770_Object_t *pObj, uint16_t RegAddr, uint16_t RegVal)
{
  if (pObj == NULL)
  {
    return LAN8770_STATUS_ERROR;
  }
  return LAN8770_WriteRegWrap(pObj, RegAddr, RegVal);
}

/**
  * @brief  Read a Clause 45 (MMD) register through the Clause 22 indirect
  *         access mechanism (registers 0x0D/0x0E), per IEEE 802.3
  *         subclause 22.2.4.3. Used for standardized BASE-T1 PMA/PMD
  *         registers such as LAN8770_BT1_CTRL_REG.
  */
int32_t LAN8770_ReadMMDReg(lan8770_Object_t *pObj, uint16_t MMDDevAddr, uint16_t RegAddr, uint32_t *pRegVal)
{
  if ((pObj == NULL) || (pRegVal == NULL))
  {
    return LAN8770_STATUS_ERROR;
  }

  if (LAN8770_WriteRegWrap(pObj, LAN8770_MMDACR,
                            (uint16_t)(LAN8770_MMDACR_FUNC_ADDRESS |
                                       (MMDDevAddr & LAN8770_MMDACR_DEVAD_MASK))) != LAN8770_STATUS_OK)
  {
    return LAN8770_STATUS_WRITE_ERROR;
  }
  if (LAN8770_WriteRegWrap(pObj, LAN8770_MMDAADR, RegAddr) != LAN8770_STATUS_OK)
  {
    return LAN8770_STATUS_WRITE_ERROR;
  }
  if (LAN8770_WriteRegWrap(pObj, LAN8770_MMDACR,
                            (uint16_t)(LAN8770_MMDACR_FUNC_DATA_NO_INCR |
                                       (MMDDevAddr & LAN8770_MMDACR_DEVAD_MASK))) != LAN8770_STATUS_OK)
  {
    return LAN8770_STATUS_WRITE_ERROR;
  }

  return LAN8770_ReadRegWrap(pObj, LAN8770_MMDAADR, pRegVal);
}

/**
  * @brief  Write a Clause 45 (MMD) register through Clause 22 indirect
  *         access. See LAN8770_ReadMMDReg().
  */
int32_t LAN8770_WriteMMDReg(lan8770_Object_t *pObj, uint16_t MMDDevAddr, uint16_t RegAddr, uint16_t RegVal)
{
  if (pObj == NULL)
  {
    return LAN8770_STATUS_ERROR;
  }

  if (LAN8770_WriteRegWrap(pObj, LAN8770_MMDACR,
                            (uint16_t)(LAN8770_MMDACR_FUNC_ADDRESS |
                                       (MMDDevAddr & LAN8770_MMDACR_DEVAD_MASK))) != LAN8770_STATUS_OK)
  {
    return LAN8770_STATUS_WRITE_ERROR;
  }
  if (LAN8770_WriteRegWrap(pObj, LAN8770_MMDAADR, RegAddr) != LAN8770_STATUS_OK)
  {
    return LAN8770_STATUS_WRITE_ERROR;
  }
  if (LAN8770_WriteRegWrap(pObj, LAN8770_MMDACR,
                            (uint16_t)(LAN8770_MMDACR_FUNC_DATA_NO_INCR |
                                       (MMDDevAddr & LAN8770_MMDACR_DEVAD_MASK))) != LAN8770_STATUS_OK)
  {
    return LAN8770_STATUS_WRITE_ERROR;
  }

  return LAN8770_WriteRegWrap(pObj, LAN8770_MMDAADR, RegVal);
}

/* ------------------------------------------------------------------------ */
/* Internal helpers: route through the registered bus IO context and adapt  */
/* its int32_t 0/nonzero return convention to lan8770_StatusTypeDef.        */
/* ------------------------------------------------------------------------ */
static int32_t LAN8770_ReadRegWrap(lan8770_Object_t *pObj, uint16_t RegAddr, uint32_t *pRegVal)
{
  if (pObj->IO.ReadReg(pObj->DevAddr, RegAddr, pRegVal) != 0)
  {
    return LAN8770_STATUS_READ_ERROR;
  }
  return LAN8770_STATUS_OK;
}

static int32_t LAN8770_WriteRegWrap(lan8770_Object_t *pObj, uint16_t RegAddr, uint16_t RegVal)
{
  if (pObj->IO.WriteReg(pObj->DevAddr, RegAddr, RegVal) != 0)
  {
    return LAN8770_STATUS_WRITE_ERROR;
  }
  return LAN8770_STATUS_OK;
}
