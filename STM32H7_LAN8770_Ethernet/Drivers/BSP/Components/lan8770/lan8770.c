/**
  ******************************************************************************
  * @file    lan8770.c
  * @brief   Microchip LAN8770 (100BASE-T1, RMII) PHY driver
  ******************************************************************************
  * 초기화 순서 (LAN8770_Init)
  *   1) MDIO 로 PHY ID 확인 (주소 자동 탐색 가능)
  *   2) 소프트 리셋 (BCR bit15)
  *   3) HW init 시작값 기록
  *   4) Master/Slave 설정 (Reg9 bit11)
  *   5) SLAVE 이면 Equalizer freeze 설정
  *   6) DSP/AFE 초기화 테이블 기록 → PCS 리셋 → HW init 완료 대기
  *
  * 3), 5), 6) 의 값은 Microchip 이 LAN87xx 용으로 공개한 초기화 값입니다
  * (Linux microchip_t1.c 의 lan87xx_phy_init() 에 같은 값이 있습니다).
  ******************************************************************************
  */

#include "lan8770.h"
#include <stddef.h>

/* ---------------------------------------------------------------------------
 *  초기화 명령 테이블
 * ------------------------------------------------------------------------ */
typedef enum
{
  CMD_WRITE = 0,
  CMD_READ  = 1,   /* 읽고 버림 (인터럽트 소스 클리어, SQI 첫 값 버리기) */
  CMD_POLL  = 2    /* SMI 레지스터의 val 비트가 0 이 될 때까지 대기 */
} lan8770_CmdMode_t;

typedef struct
{
  uint8_t  mode;
  uint8_t  bank;
  uint8_t  offset;
  uint16_t val;
} lan8770_Cmd_t;

/* TX 파워다운 + HW_Init High + Force ED */
static const lan8770_Cmd_t hw_init_seq[] =
{
  { CMD_WRITE, LAN8770_BANK_AFE, 0x0B, 0x002D },   /* T1_AFE_PORT_CFG1 : TXPD / TXAMP6 */
  { CMD_WRITE, LAN8770_BANK_SMI, 0x1A, 0x0308 },   /* POWER_DOWN_CONTROL               */
};

/* SLAVE 일 때만 : Equalizer Full-Duplex freeze */
static const lan8770_Cmd_t slave_init_seq[] =
{
  { CMD_WRITE, LAN8770_BANK_DSP, 0x69, 0x0002 },
  { CMD_WRITE, LAN8770_BANK_DSP, 0x6A, 0x0002 },
  { CMD_WRITE, LAN8770_BANK_DSP, 0x6B, 0x0002 },
  { CMD_WRITE, LAN8770_BANK_DSP, 0x6C, 0x0002 },
  { CMD_WRITE, LAN8770_BANK_DSP, 0x6D, 0x0002 },
  { CMD_WRITE, LAN8770_BANK_DSP, 0x6E, 0x0002 },
};

static const lan8770_Cmd_t phy_init_seq[] =
{
  /* Slave Full Duplex Multi */
  { CMD_WRITE, LAN8770_BANK_DSP, 0x18, 0x0D53 },
  /* CDR Pre / Post lock */
  { CMD_WRITE, LAN8770_BANK_DSP, 0x05, 0x0AB2 },
  { CMD_WRITE, LAN8770_BANK_DSP, 0x06, 0x0AB3 },
  /* Lock stage 2~3 multi factor */
  { CMD_WRITE, LAN8770_BANK_DSP, 0x1A, 0x0AEA },
  { CMD_WRITE, LAN8770_BANK_DSP, 0x1B, 0x0AEB },
  { CMD_WRITE, LAN8770_BANK_DSP, 0x1C, 0x0AEB },
  /* Pointer delay */
  { CMD_WRITE, LAN8770_BANK_DSP, 0x02, 0x1C00 },
  /* TX IIR 계수 */
  { CMD_WRITE, LAN8770_BANK_DSP, 0x55, 0x1000 },
  { CMD_WRITE, LAN8770_BANK_DSP, 0x55, 0x1861 },
  { CMD_WRITE, LAN8770_BANK_DSP, 0x55, 0x1061 },
  { CMD_WRITE, LAN8770_BANK_DSP, 0x55, 0x1922 },
  { CMD_WRITE, LAN8770_BANK_DSP, 0x55, 0x1122 },
  { CMD_WRITE, LAN8770_BANK_DSP, 0x55, 0x1983 },
  { CMD_WRITE, LAN8770_BANK_DSP, 0x55, 0x1183 },
  { CMD_WRITE, LAN8770_BANK_DSP, 0x55, 0x1944 },
  { CMD_WRITE, LAN8770_BANK_DSP, 0x55, 0x1144 },
  { CMD_WRITE, LAN8770_BANK_DSP, 0x55, 0x18C5 },
  { CMD_WRITE, LAN8770_BANK_DSP, 0x55, 0x10C5 },
  { CMD_WRITE, LAN8770_BANK_DSP, 0x55, 0x1846 },
  { CMD_WRITE, LAN8770_BANK_DSP, 0x55, 0x1046 },
  { CMD_WRITE, LAN8770_BANK_DSP, 0x55, 0x1807 },
  { CMD_WRITE, LAN8770_BANK_DSP, 0x55, 0x1007 },
  { CMD_WRITE, LAN8770_BANK_DSP, 0x55, 0x1808 },
  { CMD_WRITE, LAN8770_BANK_DSP, 0x55, 0x1008 },
  { CMD_WRITE, LAN8770_BANK_DSP, 0x55, 0x1809 },
  { CMD_WRITE, LAN8770_BANK_DSP, 0x55, 0x1009 },
  { CMD_WRITE, LAN8770_BANK_DSP, 0x55, 0x180A },
  { CMD_WRITE, LAN8770_BANK_DSP, 0x55, 0x100A },
  { CMD_WRITE, LAN8770_BANK_DSP, 0x55, 0x180B },
  { CMD_WRITE, LAN8770_BANK_DSP, 0x55, 0x100B },
  { CMD_WRITE, LAN8770_BANK_DSP, 0x55, 0x180C },
  { CMD_WRITE, LAN8770_BANK_DSP, 0x55, 0x100C },
  { CMD_WRITE, LAN8770_BANK_DSP, 0x55, 0x180D },
  { CMD_WRITE, LAN8770_BANK_DSP, 0x55, 0x100D },
  { CMD_WRITE, LAN8770_BANK_DSP, 0x55, 0x180E },
  { CMD_WRITE, LAN8770_BANK_DSP, 0x55, 0x100E },
  { CMD_WRITE, LAN8770_BANK_DSP, 0x55, 0x180F },
  { CMD_WRITE, LAN8770_BANK_DSP, 0x55, 0x100F },
  { CMD_WRITE, LAN8770_BANK_DSP, 0x55, 0x1810 },
  { CMD_WRITE, LAN8770_BANK_DSP, 0x55, 0x1010 },
  { CMD_WRITE, LAN8770_BANK_DSP, 0x55, 0x1811 },
  { CMD_WRITE, LAN8770_BANK_DSP, 0x55, 0x1011 },
  { CMD_WRITE, LAN8770_BANK_DSP, 0x55, 0x1000 },
  /* SQI 측정 설정 */
  { CMD_WRITE, LAN8770_BANK_DSP, 0x04, 0x16D6 },   /* COEF_CLK_PWR_DN  */
  { CMD_WRITE, LAN8770_BANK_DSP, 0x2E, 0x9572 },   /* SQI enable       */
  { CMD_WRITE, LAN8770_BANK_DSP, 0x4A, 0x0001 },   /* SQI mode 5       */
  { CMD_WRITE, LAN8770_BANK_DSP, 0x0D, 0x0301 },   /* 첫 SQI 값 버리기 */
  { CMD_READ,  LAN8770_BANK_DSP, 0xC3, 0      },
  /* LPS / WUR 을 idle error 로 처리 */
  { CMD_WRITE, LAN8770_BANK_SMI, 0x10, 0x0014 },
  /* HW_Init 토글, Force ED 해제, TXPD off */
  { CMD_WRITE, LAN8770_BANK_SMI, 0x1A, 0x0200 },
  /* PCS 리셋 → 하드웨어 초기화 시작 */
  { CMD_WRITE, LAN8770_BANK_SMI, 0x10, 0x0094 },
  /* 하드웨어 초기화 완료 대기 (bit7 → 0) */
  { CMD_POLL,  LAN8770_BANK_SMI, 0x10, LAN8770_MDIO_CONTROL2_HW_INIT_BUSY },
  /* TX 진폭 */
  { CMD_WRITE, LAN8770_BANK_AFE, 0x0B, 0x000C },
  /* 인터럽트 소스 클리어 */
  { CMD_READ,  LAN8770_BANK_SMI, 0x18, 0      },
  { CMD_READ,  LAN8770_BANK_MISC, 0x08, 0     },
  /* HW_Init High */
  { CMD_WRITE, LAN8770_BANK_SMI, 0x1A, 0x0300 },
};

#define ARRAY_LEN(a)  (sizeof(a) / sizeof((a)[0]))

/* ---------------------------------------------------------------------------
 *  내부 함수
 * ------------------------------------------------------------------------ */
static int32_t smi_read(lan8770_Object_t *pObj, uint32_t reg, uint32_t *pVal)
{
  if (pObj->IO.ReadReg(pObj->DevAddr, reg, pVal) < 0)
  {
    return LAN8770_STATUS_READ_ERROR;
  }
  return LAN8770_STATUS_OK;
}

static int32_t smi_write(lan8770_Object_t *pObj, uint32_t reg, uint32_t val)
{
  if (pObj->IO.WriteReg(pObj->DevAddr, reg, val) < 0)
  {
    return LAN8770_STATUS_WRITE_ERROR;
  }
  return LAN8770_STATUS_OK;
}

static int32_t smi_modify(lan8770_Object_t *pObj, uint32_t reg, uint16_t clr, uint16_t set)
{
  uint32_t val = 0;
  int32_t  st  = smi_read(pObj, reg, &val);

  if (st != LAN8770_STATUS_OK)
  {
    return st;
  }
  val = (val & ~(uint32_t)clr) | set;
  return smi_write(pObj, reg, val);
}

/* SMI 레지스터의 mask 비트가 모두 0 이 될 때까지 대기 */
static int32_t smi_poll_clear(lan8770_Object_t *pObj, uint32_t reg, uint16_t mask, uint32_t timeout_ms)
{
  uint32_t val   = 0;
  uint32_t start = (uint32_t)pObj->IO.GetTick();
  int32_t  st;

  do
  {
    st = smi_read(pObj, reg, &val);
    if (st != LAN8770_STATUS_OK)
    {
      return st;
    }
    if ((val & mask) == 0U)
    {
      return LAN8770_STATUS_OK;
    }
  } while (((uint32_t)pObj->IO.GetTick() - start) <= timeout_ms);

  return LAN8770_STATUS_RESET_TIMEOUT;
}

static int32_t run_seq(lan8770_Object_t *pObj, const lan8770_Cmd_t *seq, uint32_t cnt)
{
  uint16_t dummy;
  int32_t  st = LAN8770_STATUS_OK;
  uint32_t i;

  for (i = 0; (i < cnt) && (st == LAN8770_STATUS_OK); i++)
  {
    switch (seq[i].mode)
    {
      case CMD_WRITE:
        st = LAN8770_WriteExtReg(pObj, seq[i].bank, seq[i].offset, seq[i].val);
        break;
      case CMD_READ:
        st = LAN8770_ReadExtReg(pObj, seq[i].bank, seq[i].offset, &dummy);
        break;
      case CMD_POLL:
        st = smi_poll_clear(pObj, seq[i].offset, seq[i].val, LAN8770_HW_INIT_TO);
        break;
      default:
        st = LAN8770_STATUS_ERROR;
        break;
    }
  }
  return st;
}

static int32_t check_id(lan8770_Object_t *pObj, uint32_t addr)
{
  uint32_t id1 = 0, id2 = 0;

  if (pObj->IO.ReadReg(addr, LAN8770_PHYI1R, &id1) < 0) return LAN8770_STATUS_READ_ERROR;
  if (pObj->IO.ReadReg(addr, LAN8770_PHYI2R, &id2) < 0) return LAN8770_STATUS_READ_ERROR;

  if ((id1 == LAN8770_PHY_ID1) && ((id2 & LAN8770_PHY_ID2_MASK) == LAN8770_PHY_ID2))
  {
    pObj->PhyRevision = id2 & 0x000FU;
    return LAN8770_STATUS_OK;
  }
  return LAN8770_STATUS_ADDRESS_ERROR;
}

/* ---------------------------------------------------------------------------
 *  공개 API
 * ------------------------------------------------------------------------ */
int32_t LAN8770_RegisterBusIO(lan8770_Object_t *pObj, lan8770_IOCtx_t *ioctx)
{
  if ((pObj == NULL) || (ioctx == NULL) ||
      (ioctx->ReadReg == NULL) || (ioctx->WriteReg == NULL) || (ioctx->GetTick == NULL))
  {
    return LAN8770_STATUS_ERROR;
  }

  pObj->IO.Init     = ioctx->Init;
  pObj->IO.DeInit   = ioctx->DeInit;
  pObj->IO.ReadReg  = ioctx->ReadReg;
  pObj->IO.WriteReg = ioctx->WriteReg;
  pObj->IO.GetTick  = ioctx->GetTick;

  return LAN8770_STATUS_OK;
}

int32_t LAN8770_Init(lan8770_Object_t *pObj, uint32_t Addr, lan8770_Role_t Role)
{
  lan8770_Role_t cur;
  uint32_t a;
  int32_t  st;

  if (pObj->Is_Initialized != 0U)
  {
    return LAN8770_STATUS_OK;
  }

  if (pObj->IO.Init != NULL)
  {
    pObj->IO.Init();
  }

  /* 1) PHY 찾기 */
  st = LAN8770_STATUS_ADDRESS_ERROR;
  if (Addr == LAN8770_ADDR_AUTO)
  {
    for (a = 0; a <= 31U; a++)
    {
      if (check_id(pObj, a) == LAN8770_STATUS_OK)
      {
        pObj->DevAddr = a;
        st = LAN8770_STATUS_OK;
        break;
      }
    }
  }
  else if (Addr <= 31U)
  {
    st = check_id(pObj, Addr);
    pObj->DevAddr = Addr;
  }
  if (st != LAN8770_STATUS_OK)
  {
    return st;
  }

  /* 2) 소프트 리셋 */
  st = LAN8770_SoftReset(pObj);
  if (st != LAN8770_STATUS_OK) return st;

  /* 3) HW init 시작 */
  st = run_seq(pObj, hw_init_seq, ARRAY_LEN(hw_init_seq));
  if (st != LAN8770_STATUS_OK) return st;

  /* 4) Master / Slave */
  if (Role != LAN8770_ROLE_KEEP)
  {
    st = smi_modify(pObj, LAN8770_MSCR, LAN8770_MSCR_AS_MASTER,
                    (Role == LAN8770_ROLE_MASTER) ? LAN8770_MSCR_AS_MASTER : 0U);
    if (st != LAN8770_STATUS_OK) return st;
  }

  /* 5) SLAVE 전용 설정 */
  st = LAN8770_GetRole(pObj, &cur);
  if (st != LAN8770_STATUS_OK) return st;
  if (cur == LAN8770_ROLE_SLAVE)
  {
    st = run_seq(pObj, slave_init_seq, ARRAY_LEN(slave_init_seq));
    if (st != LAN8770_STATUS_OK) return st;
  }

  /* 6) DSP / AFE 초기화 + PCS 리셋 */
  st = run_seq(pObj, phy_init_seq, ARRAY_LEN(phy_init_seq));
  if (st != LAN8770_STATUS_OK) return st;

  pObj->Is_Initialized = 1U;
  return LAN8770_STATUS_OK;
}

int32_t LAN8770_DeInit(lan8770_Object_t *pObj)
{
  if (pObj->Is_Initialized != 0U)
  {
    if ((pObj->IO.DeInit != NULL) && (pObj->IO.DeInit() < 0))
    {
      return LAN8770_STATUS_ERROR;
    }
    pObj->Is_Initialized = 0U;
  }
  return LAN8770_STATUS_OK;
}

int32_t LAN8770_SoftReset(lan8770_Object_t *pObj)
{
  int32_t st = smi_write(pObj, LAN8770_BCR, LAN8770_BCR_SOFT_RESET);

  if (st != LAN8770_STATUS_OK)
  {
    return st;
  }
  return smi_poll_clear(pObj, LAN8770_BCR, LAN8770_BCR_SOFT_RESET, LAN8770_SW_RESET_TO);
}

int32_t LAN8770_GetLinkState(lan8770_Object_t *pObj)
{
  uint32_t val = 0;

  if (smi_read(pObj, LAN8770_T1_MODE_STAT, &val) != LAN8770_STATUS_OK)
  {
    return LAN8770_STATUS_READ_ERROR;
  }
  /* 100BASE-T1 은 오토네고가 없으므로 링크가 있으면 항상 100M Full */
  return ((val & LAN8770_T1_MODE_STAT_LINK_UP) != 0U) ?
         LAN8770_STATUS_100MBITS_FULLDUPLEX : LAN8770_STATUS_LINK_DOWN;
}

/* 동작 중 역할 변경 : 레지스터를 바꾸고 전체 초기화를 다시 수행 */
int32_t LAN8770_SetRole(lan8770_Object_t *pObj, lan8770_Role_t Role)
{
  uint32_t addr = pObj->DevAddr;

  pObj->Is_Initialized = 0U;
  return LAN8770_Init(pObj, addr, Role);
}

int32_t LAN8770_GetRole(lan8770_Object_t *pObj, lan8770_Role_t *pRole)
{
  uint32_t val = 0;
  int32_t  st  = smi_read(pObj, LAN8770_MSCR, &val);

  if (st == LAN8770_STATUS_OK)
  {
    *pRole = ((val & LAN8770_MSCR_AS_MASTER) != 0U) ? LAN8770_ROLE_MASTER : LAN8770_ROLE_SLAVE;
  }
  return st;
}

/* SQI : 0(나쁨) ~ 7(좋음). 링크가 올라와 있을 때만 의미가 있음 */
int32_t LAN8770_GetSQI(lan8770_Object_t *pObj, uint8_t *pSqi)
{
  uint16_t val = 0;
  int32_t  st;

  st = LAN8770_WriteExtReg(pObj, LAN8770_BANK_DSP, LAN8770_DSP_COEF_RW_CTL, 0x0301U);
  if (st != LAN8770_STATUS_OK) return st;

  st = LAN8770_ReadExtReg(pObj, LAN8770_BANK_DSP, LAN8770_DSP_DCQ_SQI, &val);
  if (st != LAN8770_STATUS_OK) return st;

  *pSqi = (uint8_t)((val >> 1) & 0x07U);
  return LAN8770_STATUS_OK;
}

int32_t LAN8770_EnablePowerDownMode(lan8770_Object_t *pObj)
{
  return smi_modify(pObj, LAN8770_BCR, 0U, LAN8770_BCR_POWER_DOWN);
}

int32_t LAN8770_DisablePowerDownMode(lan8770_Object_t *pObj)
{
  return smi_modify(pObj, LAN8770_BCR, LAN8770_BCR_POWER_DOWN, 0U);
}

int32_t LAN8770_EnableLoopbackMode(lan8770_Object_t *pObj)
{
  return smi_modify(pObj, LAN8770_BCR, 0U, LAN8770_BCR_LOOPBACK);
}

int32_t LAN8770_DisableLoopbackMode(lan8770_Object_t *pObj)
{
  return smi_modify(pObj, LAN8770_BCR, LAN8770_BCR_LOOPBACK, 0U);
}

/* 링크 Down 은 SMI 0x19, 링크 Up(Comm Ready) 은 MISC 0x09 로 인터럽트 허용 */
int32_t LAN8770_EnableLinkIT(lan8770_Object_t *pObj)
{
  uint32_t src;
  int32_t  st;

  st = LAN8770_DisableLinkIT(pObj);
  if (st != LAN8770_STATUS_OK) return st;

  st = LAN8770_ClearIT(pObj, &src);
  if (st != LAN8770_STATUS_OK) return st;

  st = smi_write(pObj, LAN8770_INT_MASK, LAN8770_INT_LINK_DOWN);
  if (st != LAN8770_STATUS_OK) return st;

  return LAN8770_WriteExtReg(pObj, LAN8770_BANK_MISC, LAN8770_MISC_INT_MASK2,
                             LAN8770_MISC_INT2_COMM_READY);
}

int32_t LAN8770_DisableLinkIT(lan8770_Object_t *pObj)
{
  int32_t st = smi_write(pObj, LAN8770_INT_MASK, 0U);

  if (st != LAN8770_STATUS_OK) return st;
  return LAN8770_WriteExtReg(pObj, LAN8770_BANK_MISC, LAN8770_MISC_INT_MASK2, 0U);
}

/* 읽으면 클리어됨. *pSource = (MISC_INT_SOURCE2 << 16) | INT_SOURCE */
int32_t LAN8770_ClearIT(lan8770_Object_t *pObj, uint32_t *pSource)
{
  uint16_t src2 = 0;
  uint32_t src1 = 0;
  int32_t  st;

  st = LAN8770_ReadExtReg(pObj, LAN8770_BANK_MISC, LAN8770_MISC_INT_SOURCE2, &src2);
  if (st != LAN8770_STATUS_OK) return st;

  st = smi_read(pObj, LAN8770_INT_SOURCE, &src1);
  if (st != LAN8770_STATUS_OK) return st;

  if (pSource != NULL)
  {
    *pSource = ((uint32_t)src2 << 16) | (src1 & 0xFFFFU);
  }
  return LAN8770_STATUS_OK;
}

int32_t LAN8770_ReadExtReg(lan8770_Object_t *pObj, uint8_t Bank, uint8_t Offset, uint16_t *pVal)
{
  uint32_t val = 0;
  int32_t  st;

  if (Bank == LAN8770_BANK_SMI)
  {
    st = smi_read(pObj, Offset, &val);
  }
  else
  {
    st = smi_write(pObj, LAN8770_EXT_REG_CTL,
                   LAN8770_EXT_REG_CTL_RD | ((uint32_t)Bank << 8) | Offset);
    if (st == LAN8770_STATUS_OK)
    {
      st = smi_read(pObj, LAN8770_EXT_REG_RD_DATA, &val);
    }
  }

  if (st == LAN8770_STATUS_OK)
  {
    *pVal = (uint16_t)val;
  }
  return st;
}

int32_t LAN8770_WriteExtReg(lan8770_Object_t *pObj, uint8_t Bank, uint8_t Offset, uint16_t Val)
{
  int32_t st;

  if (Bank == LAN8770_BANK_SMI)
  {
    return smi_write(pObj, Offset, Val);
  }

  st = smi_write(pObj, LAN8770_EXT_REG_WR_DATA, Val);
  if (st != LAN8770_STATUS_OK)
  {
    return st;
  }
  return smi_write(pObj, LAN8770_EXT_REG_CTL,
                   LAN8770_EXT_REG_CTL_WR | ((uint32_t)Bank << 8) | Offset);
}
