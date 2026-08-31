# `Core/Src/freertos.c` USER CODE 스니펫

## 1) `USER CODE BEGIN Includes`

```c
/* USER CODE BEGIN Includes */
#include "app_main.h"
#include "main.h"
/* USER CODE END Includes */
```

## 2) `USER CODE BEGIN StartDefaultTask`

```c
/* USER CODE BEGIN StartDefaultTask */
(void)argument;

App_Main();          /* 반환하지 않음 : 나머지 태스크 생성 후 하트비트 루프 */

for (;;)
{
  osDelay(1000);
}
/* USER CODE END StartDefaultTask */
```

## 3) `USER CODE BEGIN 4` — FreeRTOS 훅

CubeMX 에서 `CHECK_FOR_STACK_OVERFLOW = Option2`,
`USE_MALLOC_FAILED_HOOK = Enabled` 로 설정했으므로 두 훅의 구현이 필요합니다.

```c
/* USER CODE BEGIN 4 */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
  (void)xTask;
  (void)pcTaskName;

  /* 어느 태스크인지 pcTaskName 을 워치에 걸고 확인할 것 */
  HAL_GPIO_WritePin(LED_ERR_GPIO_Port, LED_ERR_Pin, GPIO_PIN_SET);
  __disable_irq();
  for (;;) { }
}

void vApplicationMallocFailedHook(void)
{
  /* configTOTAL_HEAP_SIZE 를 키우거나 큐/스택 크기를 줄일 것 */
  HAL_GPIO_WritePin(LED_ERR_GPIO_Port, LED_ERR_Pin, GPIO_PIN_SET);
  __disable_irq();
  for (;;) { }
}
/* USER CODE END 4 */
```

> CubeMX 버전에 따라 위 두 훅의 빈 구현이 자동 생성되기도 합니다.
> 이미 있으면 본문만 위 내용으로 교체하십시오. **중복 정의하면 링크 에러**가 납니다.
