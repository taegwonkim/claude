/*
 * usart_usercode_reference.c
 *
 * CubeMX가 생성한 Core/Src/usart.c 하단에는 "USER CODE BEGIN 1" /
 * "USER CODE END 1" 마커가 있다(MX_USART1_UART_Init() 함수 다음, 파일
 * 맨 끝). ESP32와의 통신을 처리하는 HAL_UART_RxCpltCallback()은 그
 * 위치에 추가한다. 이렇게 하면 Project > Generate Code를 다시 실행해도
 * 이 콜백이 지워지지 않는다.
 *
 * 이 파일은 컴파일 대상이 아니다(#if 0으로 감싸져 있다). 아래 내용을
 * 그대로 Core/Src/usart.c의 "USER CODE BEGIN 1" 구역에 옮겨 넣으면 된다.
 *
 * 프로젝트에서 USART1 외 다른 UART도 인터럽트로 쓰고 있다면, 아래처럼
 * huart->Instance로 분기해서 각자의 콜백으로 나눠주면 된다.
 */

#if 0

/* 파일 상단, MX_USART1_UART_Init() 함수보다 앞에 있는 구역 */
/* USER CODE BEGIN 0 */
#include "esp32_at.h"
/* USER CODE END 0 */

/* 파일 맨 아래, MX_USART1_UART_Init()/HAL_UART_MspInit() 다음 구역 */
/* USER CODE BEGIN 1 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART1)
  {
    ESP32_AT_UART_RxCpltCallback(huart);
  }
}
/* USER CODE END 1 */

#endif /* #if 0 */
