/* =========================================================
 * FDCAN 프로토콜 구현
 *
 * STM32L5 FDCAN1:
 *  - Nominal: 500 kbps  (Classic CAN / FDCAN 공통)
 *  - Data   : 2 Mbps    (FDCAN BRS 모드)
 *  - FIFO0  : RX (명령 수신)
 *  - TX 버퍼: Dedicated TX buffer 3개
 *
 * FDCAN 클럭 계산 (PCLK1 = 80 MHz):
 *  Nominal 500 kbps:
 *    Prescaler=10, NomTimeSeg1=11, NomTimeSeg2=4, SJW=4
 *    TQ = 80MHz/10 = 8MHz, bit time = 1+11+4 = 16TQ
 *    Rate = 8MHz / 16 = 500 kbps ✓
 *
 *  Data 2 Mbps:
 *    Prescaler=2, DataTimeSeg1=14, DataTimeSeg2=5, DataSJW=4
 *    TQ = 80MHz/2 = 40MHz, bit time = 1+14+5 = 20TQ
 *    Rate = 40MHz / 20 = 2 Mbps ✓
 * =========================================================*/

#include "fdcan_protocol.h"
#include "uart_protocol.h"   /* Proto_SendEvent (폴트 이벤트 UART 동시 전송) */
#include "cmsis_os2.h"
#include <string.h>
#include <stdio.h>

/* ----- FDCAN 핸들 정의 ----- */
FDCAN_HandleTypeDef hfdcan1;

/* ----- FreeRTOS 객체 (freertos.c에서도 extern 선언 필요) ----- */
extern osMessageQueueId_t xCmdQueueHandle;   /* FDCAN RX → 명령 큐 (UART와 공유) */
extern osMessageQueueId_t xRespQueueHandle;  /* 응답 큐 (UART 동시 전송) */

/* FDCAN RX 전용 큐 */
osMessageQueueId_t xFdcanRxQueueHandle;
static const osMessageQueueAttr_t xFdcanRxQueue_attr = { .name = "xFdcanRxQ" };

/* FDCAN TX 뮤텍스 */
static osMutexId_t xFdcanTxMutexHandle;
static const osMutexAttr_t xFdcanTxMutex_attr = {
    .name      = "xFdcanTxMtx",
    .attr_bits = osMutexPrioInherit,
};

/* TX 타임스탬프 추적 */
static uint32_t last_tx_tick = 0;

/* =========================================================
 * 내부 유틸: uint16 Big-Endian 인코딩
 * =========================================================*/
static inline void _pack_u16_be(uint8_t *buf, uint16_t val)
{
    buf[0] = (uint8_t)(val >> 8);
    buf[1] = (uint8_t)(val & 0xFF);
}

static inline void _pack_i16_be(uint8_t *buf, int16_t val)
{
    _pack_u16_be(buf, (uint16_t)val);
}

static inline uint16_t _unpack_u16_be(const uint8_t *buf)
{
    return ((uint16_t)buf[0] << 8) | buf[1];
}

/* =========================================================
 * FDCAN HAL MspInit (GPIO + 클럭)
 * main.c의 HAL_FDCAN_MspInit 콜백에서 호출
 * =========================================================*/
void HAL_FDCAN_MspInit(FDCAN_HandleTypeDef *hfdcan)
{
    GPIO_InitTypeDef gpio = {0};

    if (hfdcan->Instance == FDCAN1) {
        __HAL_RCC_FDCAN_CLK_ENABLE();
        __HAL_RCC_GPIOB_CLK_ENABLE();

        /* PB8 = FDCAN1_RX, PB9 = FDCAN1_TX (AF9) */
        gpio.Pin       = GPIO_PIN_8 | GPIO_PIN_9;
        gpio.Mode      = GPIO_MODE_AF_PP;
        gpio.Pull      = GPIO_NOPULL;
        gpio.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
        gpio.Alternate = GPIO_AF9_FDCAN1;
        HAL_GPIO_Init(GPIOB, &gpio);

        /* NVIC: FDCAN1 인터럽트 (RX FIFO0) */
        HAL_NVIC_SetPriority(FDCAN1_IT0_IRQn, 6, 0);
        HAL_NVIC_EnableIRQ(FDCAN1_IT0_IRQn);
    }
}

void HAL_FDCAN_MspDeInit(FDCAN_HandleTypeDef *hfdcan)
{
    if (hfdcan->Instance == FDCAN1) {
        __HAL_RCC_FDCAN_FORCE_RESET();
        __HAL_RCC_FDCAN_RELEASE_RESET();
        HAL_GPIO_DeInit(GPIOB, GPIO_PIN_8 | GPIO_PIN_9);
        HAL_NVIC_DisableIRQ(FDCAN1_IT0_IRQn);
    }
}

/* =========================================================
 * FDCAN 초기화
 *   - Nominal 500 kbps
 *   - Data    2 Mbps (FDCAN 프레임 BRS)
 *   - Classic CAN 프레임도 동시 지원
 * =========================================================*/
HAL_StatusTypeDef FdcanProto_Init(void)
{
    FDCAN_FilterTypeDef filter = {0};

    hfdcan1.Instance                  = FDCAN1;
    hfdcan1.Init.ClockDivider         = FDCAN_CLOCK_DIV1;
    hfdcan1.Init.FrameFormat          = FDCAN_FRAME_FD_BRS; /* FDCAN + BRS 허용 */
    hfdcan1.Init.Mode                 = FDCAN_MODE_NORMAL;
    hfdcan1.Init.AutoRetransmission   = ENABLE;   /* 송신 실패 시 자동 재전송 */
    hfdcan1.Init.TransmitPause        = DISABLE;
    hfdcan1.Init.ProtocolException    = ENABLE;

    /* ----- Nominal 비트타이밍 (500 kbps) ----- */
    /* FDCAN 클럭 = PCLK1 = 80MHz (CubeMX에서 FDCAN 클럭 소스 확인) */
    hfdcan1.Init.NominalPrescaler     = 10;   /* 80MHz / 10 = 8MHz  */
    hfdcan1.Init.NominalSyncJumpWidth = 4;
    hfdcan1.Init.NominalTimeSeg1      = 11;   /* prop + phase1      */
    hfdcan1.Init.NominalTimeSeg2      = 4;    /* phase2             */
    /* 비트레이트 = 8MHz / (1+11+4) = 500 kbps ✓ */

    /* ----- Data 비트타이밍 (2 Mbps, FDCAN BRS) ----- */
    hfdcan1.Init.DataPrescaler        = 2;    /* 80MHz / 2 = 40MHz  */
    hfdcan1.Init.DataSyncJumpWidth    = 4;
    hfdcan1.Init.DataTimeSeg1         = 14;
    hfdcan1.Init.DataTimeSeg2         = 5;
    /* 비트레이트 = 40MHz / (1+14+5) = 2 Mbps ✓ */

    /* ----- 메시지 RAM 설정 ----- */
    hfdcan1.Init.StdFiltersNbr        = 4;    /* Standard ID 필터 4개 */
    hfdcan1.Init.ExtFiltersNbr        = 0;
    hfdcan1.Init.RxFifo0ElmtsNbr     = 8;    /* RX FIFO0: 8 슬롯 */
    hfdcan1.Init.RxFifo0ElmtSize     = FDCAN_DATA_BYTES_8;
    hfdcan1.Init.RxFifo1ElmtsNbr     = 0;
    hfdcan1.Init.RxBuffersNbr        = 0;
    hfdcan1.Init.TxEventsNbr         = 0;
    hfdcan1.Init.TxBuffersNbr        = 3;    /* Dedicated TX 버퍼 3개 */
    hfdcan1.Init.TxFifoQueueElmtsNbr = 0;
    hfdcan1.Init.TxFifoQueueMode     = FDCAN_TX_FIFO_OPERATION;
    hfdcan1.Init.TxElmtSize          = FDCAN_DATA_BYTES_8;

    if (HAL_FDCAN_Init(&hfdcan1) != HAL_OK) return HAL_ERROR;

    /* ----- RX 필터 설정 ----- */
    /* 수신 대상: 0x300~0x303 (PC 명령 4종) */
    /* 필터0: 0x300 (SET)    → FIFO0 */
    filter.IdType       = FDCAN_STANDARD_ID;
    filter.FilterIndex  = 0;
    filter.FilterType   = FDCAN_FILTER_DUAL;  /* 두 ID 동시 허용 */
    filter.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
    filter.FilterID1    = FDCAN_ID_CMD_SET;   /* 0x300 */
    filter.FilterID2    = FDCAN_ID_CMD_GET;   /* 0x301 */
    if (HAL_FDCAN_ConfigFilter(&hfdcan1, &filter) != HAL_OK) return HAL_ERROR;

    /* 필터1: 0x302~0x303 (ENABLE, RESET) → FIFO0 */
    filter.FilterIndex  = 1;
    filter.FilterID1    = FDCAN_ID_CMD_ENABLE; /* 0x302 */
    filter.FilterID2    = FDCAN_ID_CMD_RESET;  /* 0x303 */
    if (HAL_FDCAN_ConfigFilter(&hfdcan1, &filter) != HAL_OK) return HAL_ERROR;

    /* 필터2, 3: 사용 안 함 (reject) */
    filter.FilterIndex  = 2;
    filter.FilterConfig = FDCAN_FILTER_REJECT_ID;
    filter.FilterID1    = 0x000; filter.FilterID2 = 0x000;
    HAL_FDCAN_ConfigFilter(&hfdcan1, &filter);
    filter.FilterIndex  = 3;
    HAL_FDCAN_ConfigFilter(&hfdcan1, &filter);

    /* 필터 미일치 메시지: 거부 */
    HAL_FDCAN_ConfigGlobalFilter(&hfdcan1,
                                  FDCAN_REJECT,  /* Non-matching standard */
                                  FDCAN_REJECT,  /* Non-matching extended */
                                  FDCAN_REJECT_REMOTE,
                                  FDCAN_REJECT_REMOTE);

    /* ----- RX FIFO0 인터럽트 활성화 ----- */
    HAL_FDCAN_ActivateNotification(&hfdcan1,
                                    FDCAN_IT_RX_FIFO0_NEW_MESSAGE, 0);

    /* ----- FDCAN 시작 ----- */
    if (HAL_FDCAN_Start(&hfdcan1) != HAL_OK) return HAL_ERROR;

    return HAL_OK;
}

/* =========================================================
 * 채널 상태 메시지 전송 (8바이트)
 * ID: 0x100 + ch (ch: 0-based)
 * =========================================================*/
HAL_StatusTypeDef FdcanProto_SendChannelStatus(VoltageCtrl_t *hctrl, uint8_t ch)
{
    if (ch >= VOLTAGE_NUM_CH) return HAL_ERROR;

    FDCAN_TxHeaderTypeDef tx_hdr = {0};
    uint8_t tx_data[8] = {0};

    VoltageCh_t *c = &hctrl->ch[ch];

    /* 메시지 헤더 */
    tx_hdr.Identifier          = FDCAN_ID_CH1_STATUS + ch; /* 0x100~0x103 */
    tx_hdr.IdType              = FDCAN_STANDARD_ID;
    tx_hdr.TxFrameType         = FDCAN_DATA_FRAME;
    tx_hdr.DataLength          = FDCAN_DLC_CODE_8_BYTES;
    tx_hdr.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    tx_hdr.BitRateSwitch       = FDCAN_BRS_OFF;   /* Classic CAN 호환 */
    tx_hdr.FDFormat            = FDCAN_CLASSIC_CAN; /* Classic 8바이트 */
    tx_hdr.TxEventFifoControl  = FDCAN_NO_TX_EVENTS;
    tx_hdr.MessageMarker       = 0;

    /* 페이로드 구성 */
    tx_data[0] = ch + 1;                                   /* 채널 번호 (1-based) */
    tx_data[1] = c->fault_flags;                           /* 상태 플래그 */
    _pack_u16_be(&tx_data[2], (uint16_t)c->target_mv);     /* 목표 전압 */
    _pack_i16_be(&tx_data[4], (int16_t)c->measured_mv);    /* 측정 전압 */
    _pack_i16_be(&tx_data[6], (int16_t)c->current_ma);     /* 측정 전류 */

    return HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &tx_hdr, tx_data);
}

/* =========================================================
 * 전체 채널 요약 메시지 전송 (16바이트, FDCAN FD 프레임)
 * ID: 0x110
 * =========================================================*/
HAL_StatusTypeDef FdcanProto_SendAllStatus(VoltageCtrl_t *hctrl)
{
    FDCAN_TxHeaderTypeDef tx_hdr = {0};
    uint8_t tx_data[16] = {0};

    tx_hdr.Identifier          = FDCAN_ID_ALL_STATUS;
    tx_hdr.IdType              = FDCAN_STANDARD_ID;
    tx_hdr.TxFrameType         = FDCAN_DATA_FRAME;
    tx_hdr.DataLength          = FDCAN_DLC_CODE_16_BYTES; /* FDCAN 16바이트 */
    tx_hdr.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    tx_hdr.BitRateSwitch       = FDCAN_BRS_ON;    /* 데이터 구간 2Mbps */
    tx_hdr.FDFormat            = FDCAN_FD_CAN;    /* FDCAN 프레임 */
    tx_hdr.TxEventFifoControl  = FDCAN_NO_TX_EVENTS;
    tx_hdr.MessageMarker       = 0;

    for (int i = 0; i < VOLTAGE_NUM_CH; i++) {
        VoltageCh_t *c = &hctrl->ch[i];
        /* 목표 전압: 0~3300mV → 0~100% → 0~255 (uint8) */
        tx_data[0 + i] = (uint8_t)((c->target_mv  * 255UL) / VOLTAGE_MAX_MV);
        /* 측정 전압: 동일 */
        tx_data[4 + i] = (uint8_t)((c->measured_mv > 0 ?
                          (uint32_t)c->measured_mv * 255UL : 0) / VOLTAGE_MAX_MV);
        /* 전류: 0~1000mA → 0~255 */
        tx_data[8 + i] = (uint8_t)((c->current_ma > 0 ?
                          (uint32_t)c->current_ma * 255UL / 1000UL : 0));
        /* 상태 플래그 */
        tx_data[12 + i] = c->fault_flags;
    }

    return HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &tx_hdr, tx_data);
}

/* =========================================================
 * 폴트 이벤트 메시지 전송 (4바이트)
 * ID: 0x200
 * =========================================================*/
HAL_StatusTypeDef FdcanProto_SendFaultEvent(uint8_t ch, FaultType_t fault,
                                             int32_t current_ma)
{
    FDCAN_TxHeaderTypeDef tx_hdr = {0};
    uint8_t tx_data[4] = {0};

    tx_hdr.Identifier          = FDCAN_ID_FAULT_EVENT;
    tx_hdr.IdType              = FDCAN_STANDARD_ID;
    tx_hdr.TxFrameType         = FDCAN_DATA_FRAME;
    tx_hdr.DataLength          = FDCAN_DLC_CODE_4_BYTES;
    tx_hdr.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    tx_hdr.BitRateSwitch       = FDCAN_BRS_OFF;
    tx_hdr.FDFormat            = FDCAN_CLASSIC_CAN;
    tx_hdr.TxEventFifoControl  = FDCAN_NO_TX_EVENTS;
    tx_hdr.MessageMarker       = 0;

    tx_data[0] = ch + 1;                                    /* 채널 (1-based) */
    tx_data[1] = (uint8_t)fault;                            /* 폴트 타입 */
    _pack_u16_be(&tx_data[2], (uint16_t)(current_ma > 0 ? current_ma : 0));

    return HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &tx_hdr, tx_data);
}

/* =========================================================
 * RX 메시지 처리 (큐에서 꺼낸 수신 아이템 파싱 및 실행)
 * =========================================================*/
void FdcanProto_ProcessRx(FdcanRxItem_t *item,
                           VoltageCtrl_t *hctrl,
                           CurrentMonitor_t *hmon)
{
    if (!item) return;

    char event_msg[64];

    switch (item->id) {

        /* ---- SET <ch> <mv> ---- */
        case FDCAN_ID_CMD_SET:
            if (item->dlc >= FDCAN_DLC_CMD_SET) {
                uint8_t  ch = item->data[0] - 1;           /* 1-based → 0-based */
                uint16_t mv = _unpack_u16_be(&item->data[2]);

                if (ch < VOLTAGE_NUM_CH && mv <= VOLTAGE_MAX_MV) {
                    VoltageCtrl_SetTarget(hctrl, ch, (int32_t)mv);
                    snprintf(event_msg, sizeof(event_msg),
                             "CAN SET CH%u %umV", ch+1, mv);
                    Proto_SendEvent("FDCAN", event_msg);
                }
            }
            break;

        /* ---- GET <ch> 또는 전체 ---- */
        case FDCAN_ID_CMD_GET:
            if (item->dlc >= FDCAN_DLC_CMD_GET) {
                uint8_t ch = item->data[0];
                if (ch == 0xFF) {
                    /* 전체 채널 즉시 전송 */
                    for (int i = 0; i < VOLTAGE_NUM_CH; i++) {
                        FdcanProto_SendChannelStatus(hctrl, i);
                    }
                    FdcanProto_SendAllStatus(hctrl);
                } else {
                    ch -= 1; /* 1-based → 0-based */
                    if (ch < VOLTAGE_NUM_CH) {
                        FdcanProto_SendChannelStatus(hctrl, ch);
                    }
                }
            }
            break;

        /* ---- ENABLE <ch> <0|1> ---- */
        case FDCAN_ID_CMD_ENABLE:
            if (item->dlc >= FDCAN_DLC_CMD_ENABLE) {
                uint8_t ch  = item->data[0];
                uint8_t val = item->data[1];

                if (ch == 0xFF) {
                    VoltageCtrl_EnableAll(hctrl, (bool)val);
                    snprintf(event_msg, sizeof(event_msg),
                             "CAN ENABLE ALL %u", val);
                } else {
                    ch -= 1;
                    if (ch < VOLTAGE_NUM_CH) {
                        VoltageCtrl_Enable(hctrl, ch, (bool)val);
                        snprintf(event_msg, sizeof(event_msg),
                                 "CAN ENABLE CH%u %u", ch+1, val);
                    }
                }
                Proto_SendEvent("FDCAN", event_msg);
            }
            break;

        /* ---- RESET <ch> ---- */
        case FDCAN_ID_CMD_RESET:
            if (item->dlc >= FDCAN_DLC_CMD_RESET) {
                uint8_t ch = item->data[0];

                if (ch == 0xFF) {
                    for (int i = 0; i < VOLTAGE_NUM_CH; i++) {
                        VoltageCtrl_Reset(hctrl, i);
                        hmon->ch[i].fault         = FAULT_NONE;
                        hmon->ch[i].fault_confirm = 0;
                    }
                    Proto_SendEvent("FDCAN", "CAN RESET ALL");
                } else {
                    ch -= 1;
                    if (ch < VOLTAGE_NUM_CH) {
                        VoltageCtrl_Reset(hctrl, ch);
                        hmon->ch[ch].fault         = FAULT_NONE;
                        hmon->ch[ch].fault_confirm = 0;
                        snprintf(event_msg, sizeof(event_msg),
                                 "CAN RESET CH%u", ch+1);
                        Proto_SendEvent("FDCAN", event_msg);
                    }
                }
            }
            break;

        default:
            break;
    }
}

/* =========================================================
 * HAL FDCAN RX FIFO0 콜백 (ISR 컨텍스트)
 * FDCAN1_IT0_IRQHandler → HAL → 이 함수
 * =========================================================*/
void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan, uint32_t RxFifo0ITs)
{
    if (hfdcan->Instance != FDCAN1) return;
    if (!(RxFifo0ITs & FDCAN_IT_RX_FIFO0_NEW_MESSAGE)) return;

    FDCAN_RxHeaderTypeDef rx_hdr;
    FdcanRxItem_t         item;
    BaseType_t            xHigherPriorityTaskWoken = pdFALSE;

    /* FIFO0에서 메시지 꺼내기 (최대 8개까지 반복) */
    while (HAL_FDCAN_GetRxMessage(hfdcan, FDCAN_RX_FIFO0,
                                   &rx_hdr, item.data) == HAL_OK) {
        item.id  = rx_hdr.Identifier;
        /* HAL DLC 코드 → 실제 바이트 수 변환 */
        switch (rx_hdr.DataLength) {
            case FDCAN_DLC_CODE_1_BYTE:  item.dlc = 1; break;
            case FDCAN_DLC_CODE_2_BYTES: item.dlc = 2; break;
            case FDCAN_DLC_CODE_3_BYTES: item.dlc = 3; break;
            case FDCAN_DLC_CODE_4_BYTES: item.dlc = 4; break;
            case FDCAN_DLC_CODE_5_BYTES: item.dlc = 5; break;
            case FDCAN_DLC_CODE_6_BYTES: item.dlc = 6; break;
            case FDCAN_DLC_CODE_7_BYTES: item.dlc = 7; break;
            case FDCAN_DLC_CODE_8_BYTES: item.dlc = 8; break;
            default:                     item.dlc = 8; break;
        }

        /* xFdcanRxQueueHandle에 삽입 (ISR → 태스크) */
        xQueueSendFromISR(xFdcanRxQueueHandle,
                          &item,
                          &xHigherPriorityTaskWoken);
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/* =========================================================
 * TASK: vFdcanTask
 *
 * 역할:
 *  1) 50ms마다 4채널 상태 + 전체 요약 CAN 프레임 전송
 *  2) RX 큐에서 수신 명령 처리
 *
 * 우선순위: osPriorityNormal (3) → UART TX보다 높고 제어루프보다 낮음
 * =========================================================*/
void vFdcanTask(void *argument)
{
    extern VoltageCtrl_t   g_vctrl;
    extern CurrentMonitor_t g_currmon;

    /* FDCAN 초기화 */
    if (FdcanProto_Init() != HAL_OK) {
        Proto_SendEvent("ERROR", "FDCAN init failed");
        /* 태스크 삭제 대신 빈 루프로 대기 (재시도 가능) */
        for (;;) osDelay(5000);
    }
    Proto_SendEvent("INFO", "FDCAN 500kbps/2Mbps OK");

    /* 뮤텍스 초기화 */
    xFdcanTxMutexHandle = osMutexNew(&xFdcanTxMutex_attr);

    /* RX 큐 초기화 */
    xFdcanRxQueueHandle = osMessageQueueNew(FDCAN_RXFIFO_DEPTH,
                                             sizeof(FdcanRxItem_t),
                                             &xFdcanRxQueue_attr);

    uint32_t last_wake = osKernelGetTickCount();

    for (;;) {
        /* ---- 1. RX 큐 처리 (논블로킹, 있는 만큼 모두) ---- */
        FdcanRxItem_t rx_item;
        while (osMessageQueueGet(xFdcanRxQueueHandle,
                                  &rx_item, NULL, 0) == osOK) {
            FdcanProto_ProcessRx(&rx_item, &g_vctrl, &g_currmon);
        }

        /* ---- 2. 주기 TX (50ms마다) ---- */
        if (osMutexAcquire(xFdcanTxMutexHandle, 10) == osOK) {

            /* 채널별 Classic CAN 상태 프레임 (각 8바이트) */
            for (int i = 0; i < VOLTAGE_NUM_CH; i++) {
                FdcanProto_SendChannelStatus(&g_vctrl, i);
                /* TX FIFO가 꽉 찰 경우를 대비한 짧은 양보 */
                osDelay(1);
            }

            /* 전체 요약 FDCAN FD 프레임 (16바이트) */
            FdcanProto_SendAllStatus(&g_vctrl);

            osMutexRelease(xFdcanTxMutexHandle);
        }

        /* ---- 3. 정확한 50ms 주기 유지 ---- */
        osDelayUntil(&last_wake, FDCAN_TX_PERIOD_MS);
        last_wake = osKernelGetTickCount();
    }
}
