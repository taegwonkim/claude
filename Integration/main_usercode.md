# `Core/Src/main.c` USER CODE 스니펫

## 1) `USER CODE BEGIN Includes`

```c
/* USER CODE BEGIN Includes */
#include "app_main.h"
#include "app_types.h"
/* USER CODE END Includes */
```

## 2) `USER CODE BEGIN 2`

`MX_*_Init()` 호출이 모두 끝난 직후, `osKernelInitialize()` 이전 구간입니다.

```c
/* USER CODE BEGIN 2 */

/* USB 트랜시버 전원 유효화 (STM32L5 필수) */
HAL_PWREx_EnableVddUSB();

/* 초기 핀 상태 : ESP32 활성, 플래시 CS 비활성, LED off */
HAL_GPIO_WritePin(ESP_EN_GPIO_Port,   ESP_EN_Pin,   GPIO_PIN_SET);
HAL_GPIO_WritePin(FLASH_CS_GPIO_Port, FLASH_CS_Pin, GPIO_PIN_SET);
HAL_GPIO_WritePin(LED_RUN_GPIO_Port,  LED_RUN_Pin,  GPIO_PIN_RESET);
HAL_GPIO_WritePin(LED_WIFI_GPIO_Port, LED_WIFI_Pin, GPIO_PIN_RESET);
HAL_GPIO_WritePin(LED_ERR_GPIO_Port,  LED_ERR_Pin,  GPIO_PIN_RESET);

/* USER CODE END 2 */
```

## 3) `USER CODE BEGIN RTOS_MUTEX`

CubeMX 는 `osKernelInitialize()` 바로 다음에 이 구간을 생성합니다.
모든 커널 오브젝트(뮤텍스/세마포어/큐/이벤트플래그)를 여기서 만듭니다.

```c
/* USER CODE BEGIN RTOS_MUTEX */
if (App_PreKernelInit() != SD_OK)
{
  Error_Handler();
}
/* USER CODE END RTOS_MUTEX */
```

> `RTOS_SEMAPHORES`, `RTOS_TIMERS`, `RTOS_QUEUES`, `RTOS_THREADS`,
> `RTOS_EVENTS` 구간은 비워 둡니다.

## 4) `USER CODE BEGIN Error_Handler_Debug`

```c
/* USER CODE BEGIN Error_Handler_Debug */
__disable_irq();
HAL_GPIO_WritePin(LED_ERR_GPIO_Port, LED_ERR_Pin, GPIO_PIN_SET);
while (1)
{
}
/* USER CODE END Error_Handler_Debug */
```

> `Error_Handler()` 는 `App_PreKernelInit()` 실패(= 힙 부족) 시에만 호출됩니다.
> 이 경우 `configTOTAL_HEAP_SIZE` 를 키우십시오.
