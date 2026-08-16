/*
 * usart_usercode_reference.c
 *
 * CubeMX가 생성한 Core/Src/usart.c 하단에는 "USER CODE BEGIN 1" /
 * "USER CODE END 1" 마커가 있다(모든 UART의 MX_xxx_UART_Init() 함수 다음,
 * 파일 맨 끝). USART1(ESP32 AT), USART2(외부 ADC 수신), USART3(PC 출력)를
 * 모두 CubeMX에서 같은 usart.c에 생성했다면, HAL_UART_RxCpltCallback()은
 * 프로젝트 전체에 하나만 존재해야 하므로 huart->Instance로 분기해서 각
 * 모듈에 넘겨준다. 이렇게 하면 Project > Generate Code를 다시 실행해도
 * 이 콜백이 지워지지 않는다.
 *
 * 이 파일은 컴파일 대상이 아니다(#if 0으로 감싸져 있다). 아래 내용을
 * 그대로 Core/Src/usart.c의 같은 이름 구역에 옮겨 넣으면 된다.
 *
 * USART3(PC 출력)은 수신을 쓰지 않고 DataReporter_TimerTick()에서
 * HAL_UART_Transmit()으로만 내보내므로, RxCpltCallback 분기에 넣을 필요가
 * 없다(원한다면 나중에 PC->STM32 명령 채널로 확장할 때 추가하면 된다).
 */

#if 0

/* 파일 상단, MX_USART1_UART_Init() 함수보다 앞에 있는 구역 */
/* USER CODE BEGIN 0 */
#include "esp32_at.h"
#include "adc_uart.h"
/* USER CODE END 0 */

/* 파일 맨 아래, 모든 MX_xxx_UART_Init()/HAL_UART_MspInit() 다음 구역 */
/* USER CODE BEGIN 1 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART1)
  {
    ESP32_AT_UART_RxCpltCallback(huart);
  }
  else if (huart->Instance == USART2)
  {
    ADC_UART_RxCpltCallback(huart);
  }
}
/* USER CODE END 1 */

#endif /* #if 0 */
