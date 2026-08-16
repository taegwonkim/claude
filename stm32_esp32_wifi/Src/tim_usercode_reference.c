/*
 * tim_usercode_reference.c
 *
 * CubeMX에서 Timers > TIM6(임의의 여유 타이머)를 1Hz 주기 인터럽트로
 * 설정했다고 가정한다(설정 방법은 README의 "STM32CubeMX 설정" 참고).
 * CubeMX가 생성한 Core/Src/tim.c 하단에도 usart.c와 동일하게
 * "USER CODE BEGIN 1" / "USER CODE END 1" 마커가 있다.
 *
 * 이 파일은 컴파일 대상이 아니다(#if 0으로 감싸져 있다). 아래 내용을
 * 그대로 Core/Src/tim.c의 같은 이름 구역에 옮겨 넣으면 된다.
 *
 * 주의: CubeMX는 MX_TIM6_Init()에서 타이머를 초기화만 하고 카운터를
 * 자동으로 시작하지 않는다. main.c의 "USER CODE BEGIN 2"에서
 * HAL_TIM_Base_Start_IT(&htim6)를 반드시 호출해야 한다
 * (main_usercode_reference.c 참고).
 */

#if 0

/* 파일 상단, MX_TIM6_Init() 함수보다 앞에 있는 구역 */
/* USER CODE BEGIN 0 */
#include "data_reporter.h"
/* USER CODE END 0 */

/* 파일 맨 아래 구역 */
/* USER CODE BEGIN 1 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM6)
  {
    DataReporter_TimerTick();
  }
}
/* USER CODE END 1 */

#endif /* #if 0 */
