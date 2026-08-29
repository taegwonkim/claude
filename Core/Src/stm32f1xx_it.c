/*
 * stm32f1xx_it.c (참고용 발췌)
 *
 * CubeMX에서 Project Manager > Advanced Settings 에서 USART1 인터럽트를
 * 활성화(또는 .ioc에서 NVIC 탭의 USART1 global interrupt 체크)하면
 * 이 파일 전체가 자동 생성되며, 아래 두 핸들러도 자동으로 채워진다.
 * 참고할 수 있도록 핵심 부분만 남겨둔다.
 */

#include "main.h"

extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;

void USART1_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart1); /* 내부에서 HAL_UART_RxCpltCallback() 호출 */
}

void USART2_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart2);
}
