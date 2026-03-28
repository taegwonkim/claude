#ifndef MCP3465R_H
#define MCP3465R_H

#include "main.h"
#include <stdint.h>
#include <stdbool.h>

/* =========================================================
 * MCP3465R 24비트 델타-시그마 ADC 드라이버
 * SPI 통신 (Mode 0,0 / CPOL=0, CPHA=0)
 * 최대 SPI 클럭: 85MHz
 * 채널: 8채널 단일종단 / 4채널 차동
 * =========================================================*/

/* ----- SPI & GPIO 핀 정의 (main.h에서 CubeMX 생성된 것 사용) ----- */
extern SPI_HandleTypeDef hspi1;
#define MCP3465R_SPI           &hspi1
#define MCP3465R_CS_GPIO       GPIOB
#define MCP3465R_CS_PIN        GPIO_PIN_6
#define MCP3465R_IRQ_GPIO      GPIOB
#define MCP3465R_IRQ_PIN       GPIO_PIN_7

/* ----- 디바이스 주소 (ADDR 핀 = GND → 0x01) ----- */
#define MCP3465R_DEV_ADDR      0x01  /* 2비트: A1=0, A0=1 (기본) */

/* ----- SPI 커맨드 바이트 형식 ----- */
/* [7:6]=DevAddr [5:2]=RegAddr [1:0]=CommandType */
#define MCP3465R_CMD(addr, reg, cmd)  (((addr) << 6) | ((reg) << 2) | (cmd))

/* Command Types */
#define MCP3465R_CMD_CONV      0x00   /* Incremental Write (사용안함) */
#define MCP3465R_CMD_STATIC_RD 0x01   /* Static Read */
#define MCP3465R_CMD_INCR_WR   0x02   /* Incremental Write */
#define MCP3465R_CMD_INCR_RD   0x03   /* Incremental Read */

/* Fast Commands (레지스터 주소 = 특별값) */
#define MCP3465R_FCMD_CONV     MCP3465R_CMD(MCP3465R_DEV_ADDR, 0x0A, 0x00)  /* 변환 시작 */
#define MCP3465R_FCMD_STANDBY  MCP3465R_CMD(MCP3465R_DEV_ADDR, 0x0B, 0x00)  /* 대기 */
#define MCP3465R_FCMD_SHUTDOWN MCP3465R_CMD(MCP3465R_DEV_ADDR, 0x0C, 0x00)  /* 종료 */
#define MCP3465R_FCMD_RESET    MCP3465R_CMD(MCP3465R_DEV_ADDR, 0x0E, 0x00)  /* 리셋 */

/* ----- 레지스터 주소 ----- */
#define MCP3465R_REG_ADCDATA   0x00   /* ADC 변환 결과 (32비트 or 24비트) */
#define MCP3465R_REG_CONFIG0   0x01   /* 설정 0 */
#define MCP3465R_REG_CONFIG1   0x02   /* 설정 1 */
#define MCP3465R_REG_CONFIG2   0x03   /* 설정 2 */
#define MCP3465R_REG_CONFIG3   0x04   /* 설정 3 */
#define MCP3465R_REG_IRQ       0x05   /* 인터럽트 플래그 */
#define MCP3465R_REG_MUX       0x06   /* 채널 MUX 선택 */
#define MCP3465R_REG_SCAN      0x07   /* 스캔 모드 설정 */
#define MCP3465R_REG_TIMER     0x08   /* 타이머 설정 */
#define MCP3465R_REG_OFFSETCAL 0x09   /* 오프셋 교정 */
#define MCP3465R_REG_GAINCAL   0x0A   /* 게인 교정 */
#define MCP3465R_REG_RESERVED1 0x0B
#define MCP3465R_REG_RESERVED2 0x0C
#define MCP3465R_REG_LOCK      0x0D   /* 레지스터 잠금 */
#define MCP3465R_REG_RESERVED3 0x0E
#define MCP3465R_REG_CRCCFG    0x0F   /* CRC 설정 */

/* ----- CONFIG0 비트 정의 ----- */
#define MCP3465R_CFG0_VREF_INT     (0x1 << 7)  /* 내부 VREF 사용 */
#define MCP3465R_CFG0_VREF_EXT     (0x0 << 7)  /* 외부 VREF 사용 */
#define MCP3465R_CFG0_CLK_INT      (0x2 << 4)  /* 내부 클럭, CLK핀 없음 */
#define MCP3465R_CFG0_CLK_EXT      (0x0 << 4)  /* 외부 클럭 */
#define MCP3465R_CFG0_CS_NONE      (0x0 << 2)  /* 전류원 없음 */
#define MCP3465R_CFG0_CS_0_9UA     (0x1 << 2)  /* 0.9μA */
#define MCP3465R_CFG0_CS_3_7UA     (0x2 << 2)  /* 3.7μA */
#define MCP3465R_CFG0_CS_15UA      (0x3 << 2)  /* 15μA */
#define MCP3465R_CFG0_MODE_SHUTDOWN (0x0)       /* 종료 */
#define MCP3465R_CFG0_MODE_STANDBY  (0x2)       /* 대기 */
#define MCP3465R_CFG0_MODE_CONV     (0x3)       /* 연속 변환 */

/* ----- CONFIG1 비트 정의 ----- */
#define MCP3465R_CFG1_PRE_1        (0x0 << 6)  /* AMCLK = MCLK/1 */
#define MCP3465R_CFG1_PRE_2        (0x1 << 6)  /* AMCLK = MCLK/2 */
#define MCP3465R_CFG1_PRE_4        (0x2 << 6)  /* AMCLK = MCLK/4 */
#define MCP3465R_CFG1_PRE_8        (0x3 << 6)  /* AMCLK = MCLK/8 */
#define MCP3465R_CFG1_OSR_32       (0x0 << 2)  /* 오버샘플링 32 */
#define MCP3465R_CFG1_OSR_64       (0x1 << 2)
#define MCP3465R_CFG1_OSR_128      (0x2 << 2)
#define MCP3465R_CFG1_OSR_256      (0x3 << 2)
#define MCP3465R_CFG1_OSR_512      (0x4 << 2)
#define MCP3465R_CFG1_OSR_1024     (0x5 << 2)
#define MCP3465R_CFG1_OSR_2048     (0x6 << 2)
#define MCP3465R_CFG1_OSR_4096     (0x7 << 2)  /* 최고 해상도, 낮은 속도 */
#define MCP3465R_CFG1_OSR_8192     (0x8 << 2)
#define MCP3465R_CFG1_OSR_16384    (0x9 << 2)
#define MCP3465R_CFG1_OSR_20480    (0xA << 2)
#define MCP3465R_CFG1_OSR_24576    (0xB << 2)
#define MCP3465R_CFG1_OSR_40960    (0xC << 2)
#define MCP3465R_CFG1_OSR_49152    (0xD << 2)
#define MCP3465R_CFG1_OSR_81920    (0xE << 2)
#define MCP3465R_CFG1_OSR_98304    (0xF << 2)  /* 최저 노이즈 */

/* ----- CONFIG2 비트 정의 ----- */
#define MCP3465R_CFG2_BOOST_0_5    (0x0 << 6)  /* 전류 부스트 0.5x */
#define MCP3465R_CFG2_BOOST_0_66   (0x1 << 6)
#define MCP3465R_CFG2_BOOST_1      (0x2 << 6)  /* 정상 */
#define MCP3465R_CFG2_BOOST_2      (0x3 << 6)  /* 2x (빠른 변환) */
#define MCP3465R_CFG2_GAIN_1_3     (0x0 << 3)  /* 게인 1/3 */
#define MCP3465R_CFG2_GAIN_1       (0x1 << 3)  /* 게인 1 */
#define MCP3465R_CFG2_GAIN_2       (0x2 << 3)
#define MCP3465R_CFG2_GAIN_4       (0x3 << 3)
#define MCP3465R_CFG2_GAIN_8       (0x4 << 3)
#define MCP3465R_CFG2_GAIN_16      (0x5 << 3)
#define MCP3465R_CFG2_GAIN_32      (0x6 << 3)
#define MCP3465R_CFG2_AZ_MUX_EN    (0x1 << 2)  /* 자동 제로 MUX */
#define MCP3465R_CFG2_AZ_REF_EN    (0x1 << 1)  /* 자동 제로 레퍼런스 */

/* ----- CONFIG3 비트 정의 ----- */
#define MCP3465R_CFG3_CONV_ONE     (0x0 << 6)  /* 1회 변환 후 Standby */
#define MCP3465R_CFG3_CONV_CONT    (0x3 << 6)  /* 연속 변환 */
#define MCP3465R_CFG3_DATA_16      (0x0 << 4)  /* 16비트 결과 */
#define MCP3465R_CFG3_DATA_32_SGN  (0x1 << 4)  /* 32비트 (부호 확장) */
#define MCP3465R_CFG3_DATA_32_CHID (0x2 << 4)  /* 32비트 (채널ID 포함) */
#define MCP3465R_CFG3_DATA_32_SGN2 (0x3 << 4)  /* 32비트 (부호 확장2) */
#define MCP3465R_CFG3_CRC_FORMAT   (0x1 << 3)
#define MCP3465R_CFG3_CRC_COM_EN   (0x1 << 2)  /* 통신 CRC 활성화 */
#define MCP3465R_CFG3_CRC_CFG_EN   (0x1 << 1)  /* 설정 CRC 활성화 */
#define MCP3465R_CFG3_OFFCAL_EN    (0x1 << 0)  /* 오프셋 교정 활성화 */

/* ----- MUX 채널 정의 (VIN+/VIN-) ----- */
#define MCP3465R_MUX_CH0    0x0
#define MCP3465R_MUX_CH1    0x1
#define MCP3465R_MUX_CH2    0x2
#define MCP3465R_MUX_CH3    0x3
#define MCP3465R_MUX_CH4    0x4
#define MCP3465R_MUX_CH5    0x5
#define MCP3465R_MUX_CH6    0x6
#define MCP3465R_MUX_CH7    0x7
#define MCP3465R_MUX_AGND   0x8
#define MCP3465R_MUX_AVDD   0x9
#define MCP3465R_MUX_REFIN_P 0xB
#define MCP3465R_MUX_REFIN_N 0xC
#define MCP3465R_MUX_TEMP_P  0xD
#define MCP3465R_MUX_TEMP_N  0xE
#define MCP3465R_MUX_VCM    0xF

#define MCP3465R_MUX(p, n)   (((p) << 4) | (n))

/* ----- SCAN 레지스터 채널 비트 ----- */
#define MCP3465R_SCAN_CH0   (1 << 0)
#define MCP3465R_SCAN_CH1   (1 << 1)
#define MCP3465R_SCAN_CH2   (1 << 2)
#define MCP3465R_SCAN_CH3   (1 << 3)
#define MCP3465R_SCAN_CH4   (1 << 4)
#define MCP3465R_SCAN_CH5   (1 << 5)
#define MCP3465R_SCAN_CH6   (1 << 6)
#define MCP3465R_SCAN_CH7   (1 << 7)
#define MCP3465R_SCAN_DIFF01 (1 << 8)  /* CH0/CH1 차동 */
#define MCP3465R_SCAN_DIFF23 (1 << 9)
#define MCP3465R_SCAN_DIFF45 (1 << 10)
#define MCP3465R_SCAN_DIFF67 (1 << 11)
/* SCAN 지연 */
#define MCP3465R_SCAN_DLY_0   (0x0 << 21)  /* 지연 없음 */
#define MCP3465R_SCAN_DLY_8   (0x1 << 21)
#define MCP3465R_SCAN_DLY_16  (0x2 << 21)
#define MCP3465R_SCAN_DLY_32  (0x3 << 21)
#define MCP3465R_SCAN_DLY_64  (0x4 << 21)
#define MCP3465R_SCAN_DLY_128 (0x5 << 21)
#define MCP3465R_SCAN_DLY_256 (0x6 << 21)
#define MCP3465R_SCAN_DLY_512 (0x7 << 21)

/* ----- 상수 정의 ----- */
#define MCP3465R_VOLTAGE_NUM_CH     4       /* 전압 측정 채널 수 */
#define MCP3465R_CURRENT_NUM_CH     4       /* 전류 측정 채널 수 */
#define MCP3465R_TOTAL_CH           8       /* 전체 채널 수 */
#define MCP3465R_VREF_MV            3300    /* VREF 전압 (mV) */
#define MCP3465R_MAX_CODE           0x7FFFFF  /* 23비트 양수 최댓값 */
#define MCP3465R_TIMEOUT_MS         100     /* SPI 타임아웃 */

/* ----- 데이터 구조체 ----- */

/* ADC 측정 결과 */
typedef struct {
    uint8_t  channel;          /* 채널 번호 (0~7) */
    int32_t  raw_code;         /* 원시 ADC 코드 */
    int32_t  voltage_mv;       /* 전압 (mV) */
    bool     is_valid;         /* 데이터 유효 여부 */
} MCP3465R_Result_t;

/* MCP3465R 드라이버 핸들 */
typedef struct {
    bool     initialized;      /* 초기화 완료 여부 */
    uint8_t  config0;          /* 현재 CONFIG0 값 */
    uint8_t  config1;          /* 현재 CONFIG1 값 */
    uint8_t  config2;          /* 현재 CONFIG2 값 */
    uint8_t  config3;          /* 현재 CONFIG3 값 */
    uint8_t  current_channel;  /* 현재 선택된 채널 */
    MCP3465R_Result_t results[MCP3465R_TOTAL_CH]; /* 각 채널 결과 */
} MCP3465R_Handle_t;

/* ----- 함수 선언 ----- */
HAL_StatusTypeDef MCP3465R_Init(MCP3465R_Handle_t *hdev);
HAL_StatusTypeDef MCP3465R_Reset(MCP3465R_Handle_t *hdev);
HAL_StatusTypeDef MCP3465R_WriteReg(MCP3465R_Handle_t *hdev, uint8_t reg, uint8_t *data, uint8_t len);
HAL_StatusTypeDef MCP3465R_ReadReg(MCP3465R_Handle_t *hdev, uint8_t reg, uint8_t *data, uint8_t len);
HAL_StatusTypeDef MCP3465R_SetChannel(MCP3465R_Handle_t *hdev, uint8_t ch_pos, uint8_t ch_neg);
HAL_StatusTypeDef MCP3465R_StartConversion(MCP3465R_Handle_t *hdev);
HAL_StatusTypeDef MCP3465R_ReadConvResult(MCP3465R_Handle_t *hdev, uint8_t channel, MCP3465R_Result_t *result);
HAL_StatusTypeDef MCP3465R_ReadAllChannels(MCP3465R_Handle_t *hdev);
bool              MCP3465R_IsDataReady(MCP3465R_Handle_t *hdev);
int32_t           MCP3465R_CodeToMillivolts(int32_t raw_code, int32_t vref_mv);

#endif /* MCP3465R_H */
