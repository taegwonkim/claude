/*
 * data_reporter.h
 *
 * 1초마다 최신 ADC 값을
 *   1) USART3로 PC에 전송 (WiFi 상태와 무관하게 항상 수행)
 *   2) WiFi로 서버에 전송 (연결되어 있을 때만 best-effort로 시도)
 * 하는 모듈.
 *
 * 왜 타이머 인터럽트가 필요한가:
 *   WiFi 재접속(AT+CWJAP 등)은 최대 20초까지 메인 루프를 블로킹할 수
 *   있다. 만약 "1초마다 PC로 전송"하는 로직을 메인 루프 안에서 단순
 *   폴링으로 구현하면, WiFi가 끊겨 재접속을 시도하는 동안 PC 전송도
 *   함께 멈춰버린다. 이를 피하기 위해 PC 전송은 하드웨어 타이머(TIM)
 *   인터럽트에서 직접 수행해 메인 루프의 상태(WiFi 재접속 등)와
 *   완전히 무관하게 만든다.
 *
 *   반면 서버 전송(AT+CIPSEND)은 응답을 기다리는 데 최대 몇 초가 걸릴
 *   수 있어 인터럽트 안에서 수행하면 안 된다. 그래서 타이머 인터럽트는
 *   "보낼 데이터가 준비됐다"는 플래그만 세우고, 실제 서버 전송은 메인
 *   루프(DataReporter_Process)가 WiFi 연결 상태를 확인한 뒤 시도한다.
 *   연결이 끊겨 있으면 그 틱의 서버 전송은 조용히 건너뛰고 다음 틱을
 *   기다린다 - ADC 수신/PC 전송 경로에는 영향이 없다.
 */

#ifndef DATA_REPORTER_H
#define DATA_REPORTER_H

#include "stm32l5xx_hal.h"

/* pc_huart: PC로 데이터를 보낼 UART 핸들 (예: &huart3) */
void DataReporter_Init(UART_HandleTypeDef *pc_huart);

/* 1Hz 타이머의 HAL_TIM_PeriodElapsedCallback()에서 해당 타이머 인스턴스일
 * 때 호출한다. 내부에서 최신 ADC 값을 읽어 PC로 즉시(블로킹, 수 ms 이내)
 * 전송하고, 서버 전송용 페이로드를 준비해 둔다. */
void DataReporter_TimerTick(void);

/* main 루프에서 매 반복 호출한다. 보류 중인 서버 전송이 있고 WiFi가
 * 연결되어 있으면 전송을 시도한다. */
void DataReporter_Process(void);

#endif /* DATA_REPORTER_H */
