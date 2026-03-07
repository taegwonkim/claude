/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    fx_stm32_sd_driver.h
  * @brief   FileX ↔ STM32H5 SDMMC1(GPDMA1) 드라이버 헤더
  *
  *  FileX가 파일 시스템 작업(읽기/쓰기/포맷)을 요청할 때 이 드라이버가
  *  SDMMC1 HAL + GPDMA1을 통해 SD 카드와 통신한다.
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef FX_STM32_SD_DRIVER_H
#define FX_STM32_SD_DRIVER_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "fx_api.h"
#include "main.h"

/* Exported defines ----------------------------------------------------------*/

/** SD 섹터 크기 (512 바이트 고정) */
#define FX_SD_SECTOR_SIZE           512U

/**
 * DMA 전송 완료 대기 최대 시간 (ms)
 * SD 카드 종류에 따라 조정 (SDHC는 5초로 충분)
 */
#define FX_SD_TRANSFER_TIMEOUT_MS   5000U

/**
 * SD 카드 Ready 대기 최대 시간 (ms)
 * 쓰기 완료 후 카드가 busy 상태에서 벗어나길 기다리는 시간
 */
#define FX_SD_READY_TIMEOUT_MS      2000U

/**
 * DMA 버퍼 정렬 크기 (STM32H5 GPDMA는 Word 단위 전송)
 * 버퍼는 반드시 4바이트 정렬 필요
 */
#define FX_SD_DMA_BUFFER_ALIGN      4U

/* Exported types ------------------------------------------------------------*/

/**
 * @brief SD 전송 완료 상태 플래그
 *        DMA 콜백에서 설정되고 드라이버 폴링 루프에서 읽힘
 */
typedef enum
{
    FX_SD_TRANSFER_IDLE    = 0,  /**< 전송 없음 / 완료 */
    FX_SD_TRANSFER_ONGOING = 1,  /**< DMA 전송 진행 중 */
    FX_SD_TRANSFER_ERROR   = 2   /**< DMA 전송 오류 */
} FX_SD_TransferState_t;

/* Exported variables --------------------------------------------------------*/
extern volatile FX_SD_TransferState_t g_sd_rx_state;
extern volatile FX_SD_TransferState_t g_sd_tx_state;

/* Exported functions --------------------------------------------------------*/

/**
 * @brief FileX 미디어 드라이버 진입점
 *        FileX가 I/O 요청 시 이 함수를 호출한다.
 * @param media_ptr  FileX 미디어 제어 블록
 */
VOID fx_stm32_sd_driver(FX_MEDIA *media_ptr);

/* 내부 콜백 (HAL callback에서 호출) */
void fx_sd_rx_complete_callback(SD_HandleTypeDef *hsd);
void fx_sd_tx_complete_callback(SD_HandleTypeDef *hsd);
void fx_sd_error_callback(SD_HandleTypeDef *hsd);

#ifdef __cplusplus
}
#endif

#endif /* FX_STM32_SD_DRIVER_H */
