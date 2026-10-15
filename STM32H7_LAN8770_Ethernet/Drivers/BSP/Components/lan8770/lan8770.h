/**
  ******************************************************************************
  * @file    lan8770.h
  * @brief   Microchip LAN8770 (100BASE-T1, RMII) PHY driver
  *          - ST BSP Component 스타일 (lan8742.c 와 같은 IOCtx 구조)
  *          - STM32 HAL ETH 의 MDIO 함수를 IOCtx 로 등록해서 사용
  ******************************************************************************
  * 100BASE-T1 특징
  *  - 오토네고 없음 : 항상 100 Mbps / Full Duplex
  *  - 링크 양 끝 중 하나는 MASTER, 다른 하나는 SLAVE 여야 링크가 올라옴
  *  - 링크 상태는 BMSR 이 아니라 T1 Mode Status(0x11) bit0 로 판단
  *
  * 레지스터/초기화 시퀀스 출처
  *  - LAN8770 데이터시트 (DS60001613) 및 Microchip 이 공개한 LAN87xx 초기화 값
  *    (Linux drivers/net/phy/microchip_t1.c 에 같은 값이 수록됨)
  *  - 실제 제품에 쓰기 전에 사용 중인 칩 리비전의 데이터시트 / 에라타와
  *    반드시 대조하세요.
  ******************************************************************************
  */

#ifndef LAN8770_H
#define LAN8770_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* ---------------------------------------------------------------------------
 *  PHY ID  (Reg2 = 0x0007, Reg3 = 0xC15x : 하위 4bit 는 리비전)
 * ------------------------------------------------------------------------ */
#define LAN8770_PHY_ID1                 ((uint16_t)0x0007U)
#define LAN8770_PHY_ID2                 ((uint16_t)0xC150U)
#define LAN8770_PHY_ID2_MASK            ((uint16_t)0xFFF0U)

/* ---------------------------------------------------------------------------
 *  IEEE 표준 레지스터 (Clause 22)
 * ------------------------------------------------------------------------ */
#define LAN8770_BCR                     ((uint16_t)0x00U)  /* Basic Control         */
#define LAN8770_BSR                     ((uint16_t)0x01U)  /* Basic Status          */
#define LAN8770_PHYI1R                  ((uint16_t)0x02U)  /* PHY Identifier 1      */
#define LAN8770_PHYI2R                  ((uint16_t)0x03U)  /* PHY Identifier 2      */
#define LAN8770_MSCR                    ((uint16_t)0x09U)  /* Master/Slave Control  */
#define LAN8770_MSSR                    ((uint16_t)0x0AU)  /* Master/Slave Status   */

#define LAN8770_BCR_SOFT_RESET          ((uint16_t)0x8000U)
#define LAN8770_BCR_LOOPBACK            ((uint16_t)0x4000U)
#define LAN8770_BCR_SPEED_SELECT        ((uint16_t)0x2000U)
#define LAN8770_BCR_AUTONEGO_EN         ((uint16_t)0x1000U)
#define LAN8770_BCR_POWER_DOWN          ((uint16_t)0x0800U)
#define LAN8770_BCR_ISOLATE             ((uint16_t)0x0400U)
#define LAN8770_BCR_DUPLEX_MODE         ((uint16_t)0x0100U)

#define LAN8770_BSR_LINK_STATUS         ((uint16_t)0x0004U)

#define LAN8770_MSCR_MANUAL_CFG_EN      ((uint16_t)0x1000U)
#define LAN8770_MSCR_AS_MASTER          ((uint16_t)0x0800U)  /* 1 = MASTER, 0 = SLAVE */
#define LAN8770_MSSR_CFG_FAULT          ((uint16_t)0x8000U)
#define LAN8770_MSSR_RESOLVED_MASTER    ((uint16_t)0x4000U)  /* 링크 Up 일 때만 유효 */

/* ---------------------------------------------------------------------------
 *  벤더 레지스터 (SMI 직접 접근)
 * ------------------------------------------------------------------------ */
#define LAN8770_MDIO_CONTROL2           ((uint16_t)0x10U)
#define LAN8770_T1_MODE_STAT            ((uint16_t)0x11U)
#define LAN8770_EXT_REG_CTL             ((uint16_t)0x14U)
#define LAN8770_EXT_REG_RD_DATA         ((uint16_t)0x15U)
#define LAN8770_EXT_REG_WR_DATA         ((uint16_t)0x16U)
#define LAN8770_INT_SOURCE              ((uint16_t)0x18U)
#define LAN8770_INT_MASK                ((uint16_t)0x19U)
#define LAN8770_POWER_DOWN_CONTROL      ((uint16_t)0x1AU)

#define LAN8770_T1_MODE_STAT_LINK_UP    ((uint16_t)0x0001U)

#define LAN8770_EXT_REG_CTL_RD          ((uint16_t)0x1000U)
#define LAN8770_EXT_REG_CTL_WR          ((uint16_t)0x0800U)

#define LAN8770_MDIO_CONTROL2_HW_INIT_BUSY ((uint16_t)0x0080U)

/* INT_SOURCE / INT_MASK (SMI 0x18 / 0x19) */
#define LAN8770_INT_LINK_DOWN           ((uint16_t)0x0002U)
#define LAN8770_INT_LINK_UP             ((uint16_t)0x0004U)

/* ---------------------------------------------------------------------------
 *  확장 레지스터 뱅크 (EXT_REG_CTL 로 간접 접근)
 * ------------------------------------------------------------------------ */
#define LAN8770_BANK_SMI                ((uint8_t)0U)
#define LAN8770_BANK_MISC               ((uint8_t)1U)
#define LAN8770_BANK_PCS                ((uint8_t)2U)
#define LAN8770_BANK_AFE                ((uint8_t)3U)
#define LAN8770_BANK_DSP                ((uint8_t)4U)

/* MISC 뱅크 */
#define LAN8770_MISC_INT_SOURCE2        ((uint8_t)0x08U)
#define LAN8770_MISC_INT_MASK2          ((uint8_t)0x09U)
#define LAN8770_MISC_INT2_COMM_READY    ((uint16_t)0x0400U)

/* DSP 뱅크 (SQI) */
#define LAN8770_DSP_COEF_RW_CTL         ((uint8_t)0x0DU)
#define LAN8770_DSP_DCQ_SQI             ((uint8_t)0xC3U)
#define LAN8770_SQI_MAX                 7U

/* ---------------------------------------------------------------------------
 *  반환값
 * ------------------------------------------------------------------------ */
#define LAN8770_STATUS_READ_ERROR             ((int32_t)-5)
#define LAN8770_STATUS_WRITE_ERROR            ((int32_t)-4)
#define LAN8770_STATUS_ADDRESS_ERROR          ((int32_t)-3)
#define LAN8770_STATUS_RESET_TIMEOUT          ((int32_t)-2)
#define LAN8770_STATUS_ERROR                  ((int32_t)-1)
#define LAN8770_STATUS_OK                     ((int32_t) 0)
#define LAN8770_STATUS_LINK_DOWN              ((int32_t) 1)
#define LAN8770_STATUS_100MBITS_FULLDUPLEX    ((int32_t) 2)

/* ---------------------------------------------------------------------------
 *  Master / Slave 설정
 * ------------------------------------------------------------------------ */
typedef enum
{
  LAN8770_ROLE_KEEP   = 0,  /* 핀 strap(하드웨어) 설정을 그대로 사용 */
  LAN8770_ROLE_MASTER = 1,
  LAN8770_ROLE_SLAVE  = 2
} lan8770_Role_t;

/* PHY 주소 자동 탐색 */
#define LAN8770_ADDR_AUTO               0xFFU

/* 타임아웃 [ms] */
#define LAN8770_SW_RESET_TO             500U
#define LAN8770_HW_INIT_TO              50U

/* ---------------------------------------------------------------------------
 *  IO 컨텍스트 (lan8742_IOCtx_t 와 동일 시그니처)
 * ------------------------------------------------------------------------ */
typedef int32_t (*lan8770_Init_Func)    (void);
typedef int32_t (*lan8770_DeInit_Func)  (void);
typedef int32_t (*lan8770_ReadReg_Func) (uint32_t, uint32_t, uint32_t *);
typedef int32_t (*lan8770_WriteReg_Func)(uint32_t, uint32_t, uint32_t);
typedef int32_t (*lan8770_GetTick_Func) (void);

typedef struct
{
  lan8770_Init_Func      Init;
  lan8770_DeInit_Func    DeInit;
  lan8770_WriteReg_Func  WriteReg;
  lan8770_ReadReg_Func   ReadReg;
  lan8770_GetTick_Func   GetTick;
} lan8770_IOCtx_t;

typedef struct
{
  uint32_t         DevAddr;         /* 찾은(또는 지정한) PHY 주소 0~31   */
  uint32_t         Is_Initialized;
  uint32_t         PhyRevision;     /* PHYI2R 하위 4bit                 */
  lan8770_IOCtx_t  IO;
  void            *pData;
} lan8770_Object_t;

/* ---------------------------------------------------------------------------
 *  API
 * ------------------------------------------------------------------------ */
int32_t LAN8770_RegisterBusIO(lan8770_Object_t *pObj, lan8770_IOCtx_t *ioctx);

/* Addr : 0~31 또는 LAN8770_ADDR_AUTO,  Role : MASTER / SLAVE / KEEP */
int32_t LAN8770_Init(lan8770_Object_t *pObj, uint32_t Addr, lan8770_Role_t Role);
int32_t LAN8770_DeInit(lan8770_Object_t *pObj);

int32_t LAN8770_SoftReset(lan8770_Object_t *pObj);
int32_t LAN8770_GetLinkState(lan8770_Object_t *pObj);
int32_t LAN8770_SetRole(lan8770_Object_t *pObj, lan8770_Role_t Role);
int32_t LAN8770_GetRole(lan8770_Object_t *pObj, lan8770_Role_t *pRole);
int32_t LAN8770_GetSQI(lan8770_Object_t *pObj, uint8_t *pSqi);

int32_t LAN8770_EnablePowerDownMode(lan8770_Object_t *pObj);
int32_t LAN8770_DisablePowerDownMode(lan8770_Object_t *pObj);
int32_t LAN8770_EnableLoopbackMode(lan8770_Object_t *pObj);
int32_t LAN8770_DisableLoopbackMode(lan8770_Object_t *pObj);

/* nINT 핀 사용 시 : 링크 Down / Comm-Ready(링크 Up) 인터럽트 */
int32_t LAN8770_EnableLinkIT(lan8770_Object_t *pObj);
int32_t LAN8770_DisableLinkIT(lan8770_Object_t *pObj);
int32_t LAN8770_ClearIT(lan8770_Object_t *pObj, uint32_t *pSource);

/* 확장 레지스터 직접 접근 (디버깅용) */
int32_t LAN8770_ReadExtReg(lan8770_Object_t *pObj, uint8_t Bank, uint8_t Offset, uint16_t *pVal);
int32_t LAN8770_WriteExtReg(lan8770_Object_t *pObj, uint8_t Bank, uint8_t Offset, uint16_t Val);

#ifdef __cplusplus
}
#endif

#endif /* LAN8770_H */
