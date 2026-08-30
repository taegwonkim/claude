/*
 * main.h
 *
 * STM32L562RCT6(STM32L5 시리즈) 기준 예제 프로젝트 헤더.
 * 실제로는 STM32CubeMX가 자동 생성하는 파일이며, 여기서는 이 예제에서
 * 필요한 최소한의 선언만 남겨두었다. CubeMX로 새 프로젝트를 생성하면
 * huart1 / huart2 핸들과 GPIO 매크로들이 이 파일에 자동으로 들어간다.
 *
 * 주의: STM32L5는 TrustZone(TZEN)을 지원한다. 이 예제는 TrustZone을
 * 비활성화(Non-TrustZone, 단일 프로젝트)한 경우를 기준으로 작성했다.
 * CubeMX 프로젝트 생성 시 Pinout & Configuration > System Core > GTZC/RCC
 * 에서 TrustZone을 활성화하지 않았는지 확인할 것.
 */

#ifndef INC_MAIN_H_
#define INC_MAIN_H_

#include "stm32l5xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/* USART1 : ESP32-C3 AT 명령용 (PA9 = TX, PA10 = RX)      */
/* USART2 : ST-Link 가상 COM 포트, 디버그 로그 출력용       */
extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;

void Error_Handler(void);

#ifdef __cplusplus
}
#endif

#endif /* INC_MAIN_H_ */
