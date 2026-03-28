#ifndef MCP3465R_H
#define MCP3465R_H

/* RTOS 버전의 mcp3465r.h와 동일한 레지스터/상수 정의.
 * osDelay() 대신 HAL_Delay() / 폴링 방식으로 동작. */

#include "main.h"
#include <stdint.h>
#include <stdbool.h>

#define MCP3465R_SPI            &hspi1
#define MCP3465R_CS_GPIO        GPIOB
#define MCP3465R_CS_PIN         GPIO_PIN_6
#define MCP3465R_IRQ_GPIO       GPIOB
#define MCP3465R_IRQ_PIN        GPIO_PIN_7
#define MCP3465R_DEV_ADDR       0x01
#define MCP3465R_TIMEOUT_MS     50

/* Command */
#define MCP3465R_CMD(a,r,c)     (((a)<<6)|((r)<<2)|(c))
#define MCP3465R_CMD_STATIC_RD  0x01
#define MCP3465R_CMD_INCR_WR    0x02
#define MCP3465R_FCMD_CONV      MCP3465R_CMD(MCP3465R_DEV_ADDR,0x0A,0x00)
#define MCP3465R_FCMD_RESET     MCP3465R_CMD(MCP3465R_DEV_ADDR,0x0E,0x00)

/* Registers */
#define MCP3465R_REG_ADCDATA    0x00
#define MCP3465R_REG_CONFIG0    0x01
#define MCP3465R_REG_CONFIG1    0x02
#define MCP3465R_REG_CONFIG2    0x03
#define MCP3465R_REG_CONFIG3    0x04
#define MCP3465R_REG_IRQ        0x05
#define MCP3465R_REG_MUX        0x06
#define MCP3465R_REG_SCAN       0x07
#define MCP3465R_REG_TIMER      0x08

/* CONFIG0 */
#define MCP3465R_CFG0_VREF_EXT  (0x0<<7)
#define MCP3465R_CFG0_CLK_INT   (0x2<<4)
#define MCP3465R_CFG0_CS_NONE   (0x0<<2)
#define MCP3465R_CFG0_MODE_CONV (0x3)

/* CONFIG1 OSR */
#define MCP3465R_CFG1_PRE_1     (0x0<<6)
#define MCP3465R_CFG1_OSR_1024  (0x5<<2)

/* CONFIG2 */
#define MCP3465R_CFG2_BOOST_1   (0x2<<6)
#define MCP3465R_CFG2_GAIN_1    (0x1<<3)
#define MCP3465R_CFG2_AZ_MUX_EN (0x1<<2)

/* CONFIG3 */
#define MCP3465R_CFG3_CONV_CONT    (0x3<<6)
#define MCP3465R_CFG3_DATA_32_CHID (0x2<<4)

/* SCAN channel bits */
#define MCP3465R_SCAN_CH0   (1<<0)
#define MCP3465R_SCAN_CH1   (1<<1)
#define MCP3465R_SCAN_CH2   (1<<2)
#define MCP3465R_SCAN_CH3   (1<<3)
#define MCP3465R_SCAN_CH4   (1<<4)
#define MCP3465R_SCAN_CH5   (1<<5)
#define MCP3465R_SCAN_CH6   (1<<6)
#define MCP3465R_SCAN_CH7   (1<<7)
#define MCP3465R_SCAN_DLY_8 (0x1<<21)

#define MCP3465R_TOTAL_CH   8
#define MCP3465R_VREF_MV    3300
#define MCP3465R_MAX_CODE   0x7FFFFF

typedef struct {
    uint8_t  channel;
    int32_t  raw_code;
    int32_t  voltage_mv;
    bool     is_valid;
} MCP3465R_Result_t;

typedef struct {
    bool     initialized;
    uint8_t  config0, config1, config2, config3;
    MCP3465R_Result_t results[MCP3465R_TOTAL_CH];
    /* Non-RTOS: SCAN 상태 머신 */
    uint8_t  scan_ch_idx;    /* 현재 읽고 있는 채널 인덱스 */
    bool     scan_busy;      /* SPI 전송 중 여부 */
} MCP3465R_Handle_t;

HAL_StatusTypeDef MCP3465R_Init(MCP3465R_Handle_t *hdev);
HAL_StatusTypeDef MCP3465R_Reset(MCP3465R_Handle_t *hdev);
HAL_StatusTypeDef MCP3465R_WriteReg(MCP3465R_Handle_t *hdev, uint8_t reg, uint8_t *data, uint8_t len);
HAL_StatusTypeDef MCP3465R_StartConversion(MCP3465R_Handle_t *hdev);
/* Non-RTOS: 단일 결과 읽기 (폴링, 인터럽트 불필요) */
HAL_StatusTypeDef MCP3465R_ReadOneResult(MCP3465R_Handle_t *hdev);
/* Non-RTOS: IRQ 감지 후 호출 → 8채널 전부 읽기 */
HAL_StatusTypeDef MCP3465R_ReadAllBlocking(MCP3465R_Handle_t *hdev);
bool              MCP3465R_IsDataReady(void);
int32_t           MCP3465R_CodeToMillivolts(int32_t raw_code, int32_t vref_mv);

#endif /* MCP3465R_H */
