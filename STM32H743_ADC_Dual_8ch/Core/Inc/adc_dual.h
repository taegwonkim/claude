/**
 * @file    adc_dual.h
 * @brief   STM32H743 ADC1/ADC2 16-bit 듀얼 인터리브 모드, 8채널,
 *          TIM6 TRGO 트리거, DMA circular → D2 SRAM(.RAM_D2 섹션)
 *
 * 구성
 *   - ADC1(마스터) : 레귤러 시퀀스 4랭크 (논리 채널 CH0, CH2, CH4, CH6)
 *   - ADC2(슬레이브): 레귤러 시퀀스 4랭크 (논리 채널 CH1, CH3, CH5, CH7)
 *   - TIM6 TRGO 한 번에 스캔 1회. 슬레이브는 각 변환을 마스터보다
 *     ADC_DUAL_TWO_SAMPLING_DELAY만큼 늦게 시작합니다(인터리브).
 *   - 두 ADC의 변환 결과 한 쌍은 공통 데이터 레지스터(ADC12_COMMON->CDR)에서
 *     32비트 워드 하나로 묶입니다(DAMDF = 0b10).
 *         bit[15:0]  = ADC1(마스터) 결과
 *         bit[31:16] = ADC2(슬레이브) 결과
 *   - 스캔 1회 = 32비트 워드 4개 = 8채널 샘플
 *
 * 한 스캔 안의 시간 순서:
 *   CH0(ADC1) → CH1(ADC2) → CH2(ADC1) → CH3(ADC2) → … → CH7(ADC2)
 */
#ifndef ADC_DUAL_H
#define ADC_DUAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32h7xx_hal.h"
#include <stdint.h>

/* ------------------------------------------------------------------------- */
/* 사용자 설정                                                                */
/* ------------------------------------------------------------------------- */

/** 논리 채널 수(ADC1 4개 + ADC2 4개). 핀 테이블이 4+4 구조이므로 바꾸지 마세요. */
#define ADC_DUAL_NUM_CH              8U
/** ADC 하나당 랭크 수(= 스캔 1회당 32비트 워드 수) */
#define ADC_DUAL_RANKS_PER_ADC       4U

/** 스캔 레이트(Hz) = 채널당 샘플링 레이트. 전체 처리량은 이 값의 8배입니다.
 *  f_ADC = 36 MHz, 16-bit, 샘플링 2.5사이클이면 변환 1회 = 11사이클 = 0.306 us.
 *  스캔 1회 ≈ 4 × 11 + 5 = 49사이클 ≈ 1.36 us이므로 이론상 최대 약 730 kHz입니다.
 *  여유를 두고 설정하세요. */
#define ADC_DUAL_SCAN_RATE_HZ        500000U

/** DMA 하프 버퍼 하나에 들어가는 스캔 수(콜백 주기 = 이 값 / 스캔 레이트)
 *  256스캔 @ 500 kHz이면 512 us마다 콜백이 호출됩니다. */
#define ADC_DUAL_SCANS_PER_HALF      256U

/** DMA 버퍼 전체 길이(32비트 워드 단위) */
#define ADC_DUAL_DMA_WORDS           (ADC_DUAL_SCANS_PER_HALF * ADC_DUAL_RANKS_PER_ADC * 2U)

/** 샘플링 시간(두 ADC 공통). 신호원 임피던스가 높으면 더 길게 설정하세요. */
#define ADC_DUAL_SAMPLING_TIME       ADC_SAMPLETIME_2CYCLES_5

/** 마스터와 슬레이브 샘플링 사이의 지연(ADC 클럭 사이클) */
#define ADC_DUAL_TWO_SAMPLING_DELAY  ADC_TWOSAMPLINGDELAY_5CYCLES

/** ADC 기준 전압(mV). 보드의 VREF+ 값 */
#define ADC_DUAL_VREF_MV             3300U

/* ------------------------------------------------------------------------- */
/* 타입                                                                       */
/* ------------------------------------------------------------------------- */

/** 하프 버퍼 1개(ADC_DUAL_SCANS_PER_HALF 스캔)의 통계 */
typedef struct
{
  uint16_t min[ADC_DUAL_NUM_CH];
  uint16_t max[ADC_DUAL_NUM_CH];
  uint16_t mean[ADC_DUAL_NUM_CH];
  uint32_t block_count;   /**< 처리를 마친 하프 버퍼 수 */
} ADC_Dual_Stats_t;

/* ------------------------------------------------------------------------- */
/* API                                                                        */
/* ------------------------------------------------------------------------- */

/** ADC 클럭(PLL2P), GPIO, DMA, ADC1/ADC2, 듀얼 모드, TIM6을 설정합니다.
 *  @retval 성공하면 HAL_OK */
HAL_StatusTypeDef ADC_Dual_Init(void);

/** 캘리브레이션 후 DMA와 TIM6을 시작합니다(이때부터 샘플링 시작). */
HAL_StatusTypeDef ADC_Dual_Start(void);

/** TIM6, ADC, DMA를 정지합니다. */
HAL_StatusTypeDef ADC_Dual_Stop(void);

/** 최신 블록 통계를 복사합니다(인터럽트에 안전). */
void ADC_Dual_GetStats(ADC_Dual_Stats_t *out);

/** ADC 오버런/DMA 오류 발생 횟수 */
uint32_t ADC_Dual_GetErrorCount(void);

/** 16비트 원시값을 mV로 변환합니다. */
static inline uint32_t ADC_Dual_RawToMilliVolt(uint16_t raw)
{
  return ((uint32_t)raw * ADC_DUAL_VREF_MV + 32767U) / 65535U;
}

/** CDR 워드 하나를 샘플 2개로 분리합니다. */
static inline void ADC_Dual_Unpack(uint32_t word, uint16_t *master, uint16_t *slave)
{
  *master = (uint16_t)(word & 0xFFFFU);
  *slave  = (uint16_t)(word >> 16);
}

/**
 * @brief  하프 버퍼가 찰 때마다 DMA 인터럽트 컨텍스트에서 호출되는 사용자 훅입니다.
 *         weak 함수이므로 사용자 코드에서 다시 정의해 실시간 처리(FFT, 필터,
 *         전송 등)를 넣으세요. 호출 시점에 D-Cache 무효화는 이미 끝나 있습니다.
 * @param  raw    묶인 데이터의 시작 주소(scans × ADC_DUAL_RANKS_PER_ADC 워드)
 * @param  scans  스캔 수(= ADC_DUAL_SCANS_PER_HALF)
 *
 *  스캔 s에서 논리 채널 ch의 샘플:
 *    word = raw[s * ADC_DUAL_RANKS_PER_ADC + ch / 2]
 *    ch가 짝수면 word & 0xFFFF,  홀수면 word >> 16
 *
 *  하프 버퍼 주기(기본 512 us)보다 충분히 짧게 끝나야 합니다.
 */
void ADC_Dual_BlockCallback(const uint32_t *raw, uint32_t scans);

#ifdef __cplusplus
}
#endif

#endif /* ADC_DUAL_H */
