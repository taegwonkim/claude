/**
 * @file    main.c
 * @brief   STM32L552RCT6 X-CUBE-EEPROM 사용 예제
 *
 * 빌드 전 체크리스트:
 *  1. STM32CubeMX에서 FLASH 옵션바이트 → Dual-bank 모드 활성화 확인
 *  2. X-CUBE-EEPROM Middleware 추가 후 eeprom_emul.c / eeprom_emul.h 생성 확인
 *  3. eeprom_emul_conf.h 의 EEPROM_START_ADDRESS가 링커 스크립트의
 *     애플리케이션 코드 영역과 겹치지 않는지 확인
 *  4. 링커 스크립트에서 EEPROM 영역을 FLASH 섹션에서 제외:
 *       FLASH (rx) : ORIGIN = 0x08000000, LENGTH = 224K  (256K - 32K 예약)
 */

#include "main.h"
#include "eeprom_manager.h"
#include <stdio.h>   /* printf (semihosting / UART retarget 필요) */

/* ─── Private function prototypes ────────────────────────────────────────── */
static void SystemClock_Config(void);
static void UART2_Init(void);
static void Error_Handler(void);
static void Demo_BasicReadWrite(void);
static void Demo_BootCounter(void);
static void Demo_WriteMultiple(void);

/* ─── UART handle (printf retarget용) ───────────────────────────────────── */
UART_HandleTypeDef huart2;

/* ─── main ───────────────────────────────────────────────────────────────── */
int main(void)
{
  /* 1. HAL 및 클록 초기화 */
  HAL_Init();
  SystemClock_Config();

  /* 2. UART 초기화 (디버그 출력) */
  UART2_Init();

  printf("\r\n=== STM32L552RCT6 EEPROM Emulation Demo ===\r\n");

  /* 3. EEPROM 에뮬레이션 초기화
   *    - 최초 부팅 시 EEPROM 영역을 포맷(EE_FORCED_ERASE)
   *    - 이미 유효한 데이터가 있으면 그대로 복구              */
  if (EE_Manager_Init() != EE_MGR_OK)
  {
    printf("[ERROR] EE_Manager_Init failed\r\n");
    Error_Handler();
  }
  printf("[OK] EEPROM emulation initialised\r\n");

  /* 4. 데모 실행 */
  Demo_BootCounter();
  Demo_BasicReadWrite();
  Demo_WriteMultiple();

  printf("\r\n=== Demo complete ===\r\n");

  while (1)
  {
    HAL_Delay(1000);
  }
}

/* ─── Demo: 부팅 횟수 카운터 ─────────────────────────────────────────────── */
static void Demo_BootCounter(void)
{
  uint32_t bootCount = 0;

  /* 재부팅해도 값이 유지됨 */
  EE_Manager_IncrementCounter(VADDR_BOOT_COUNT, &bootCount);
  printf("[BOOT] Boot count: %lu\r\n", bootCount);
}

/* ─── Demo: 단순 읽기/쓰기 ──────────────────────────────────────────────── */
static void Demo_BasicReadWrite(void)
{
  uint32_t readBack = 0;

  /* --- 쓰기 --- */
  const uint32_t deviceId = 0x12345678UL;
  if (EE_Manager_Write(VADDR_DEVICE_ID, deviceId) != EE_MGR_OK)
  {
    printf("[ERROR] Write VADDR_DEVICE_ID failed\r\n");
    return;
  }
  printf("[WRITE] DEVICE_ID = 0x%08lX\r\n", deviceId);

  /* --- 읽기 --- */
  EE_MgrStatus_t st = EE_Manager_Read(VADDR_DEVICE_ID, &readBack);
  if (st == EE_MGR_OK)
  {
    printf("[READ ] DEVICE_ID = 0x%08lX  %s\r\n",
           readBack,
           (readBack == deviceId) ? "(match)" : "(MISMATCH!)");
  }
  else if (st == EE_MGR_NOT_FOUND)
  {
    printf("[READ ] VADDR_DEVICE_ID not written yet\r\n");
  }
  else
  {
    printf("[ERROR] Read failed\r\n");
  }

  /* --- 센서 캘리브레이션 예시 --- */
  const uint32_t calib = 0xABCD0001UL;
  EE_Manager_Write(VADDR_SENSOR_CALIB, calib);

  EE_Manager_Read(VADDR_SENSOR_CALIB, &readBack);
  printf("[READ ] SENSOR_CALIB = 0x%08lX\r\n", readBack);
}

/* ─── Demo: 여러 변수 한꺼번에 쓰기 ─────────────────────────────────────── */
static void Demo_WriteMultiple(void)
{
  const EE_Pair_t pairs[] = {
    { VADDR_FIRMWARE_VER,  0x00010200UL },  /* v1.2.0 */
    { VADDR_CONFIG_FLAGS,  0x00000003UL },  /* flag bits */
    { VADDR_LAST_ERROR,    0x00000000UL },  /* clear error */
  };
  const uint32_t pairCount = sizeof(pairs) / sizeof(pairs[0]);

  if (EE_Manager_WriteMultiple(pairs, pairCount) != EE_MGR_OK)
  {
    printf("[ERROR] WriteMultiple failed\r\n");
    return;
  }

  /* 읽어서 확인 */
  for (uint32_t i = 0; i < pairCount; i++)
  {
    uint32_t val = 0;
    EE_Manager_Read(pairs[i].addr, &val);
    printf("[READ ] addr=0x%04X -> 0x%08lX\r\n", pairs[i].addr, val);
  }
}

/* ─── SystemClock_Config ─────────────────────────────────────────────────── */
/**
 * STM32L552RCT6 기본 클록: MSI 4MHz → PLL → SYSCLK 110MHz
 * (CubeMX 생성 코드를 그대로 사용하거나 아래를 참고해 수정)
 */
static void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /* 전압 스케일 0: 최대 110MHz */
  __HAL_RCC_PWR_CLK_ENABLE();
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE0);

  /* MSI 48MHz → PLL 110MHz */
  RCC_OscInitStruct.OscillatorType      = RCC_OSCILLATORTYPE_MSI;
  RCC_OscInitStruct.MSIState            = RCC_MSI_ON;
  RCC_OscInitStruct.MSICalibrationValue = RCC_MSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.MSIClockRange       = RCC_MSIRANGE_11; /* 48MHz */
  RCC_OscInitStruct.PLL.PLLState        = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource       = RCC_PLLSOURCE_MSI;
  RCC_OscInitStruct.PLL.PLLM            = 6;
  RCC_OscInitStruct.PLL.PLLN            = 55;
  RCC_OscInitStruct.PLL.PLLP            = RCC_PLLP_DIV7;
  RCC_OscInitStruct.PLL.PLLQ            = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR            = RCC_PLLR_DIV2;

  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType      = (RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                      RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
  RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  /* 110MHz에서 FLASH latency = 5 WS */
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/* ─── UART2 초기화 (PA2/PA3, 115200 bps) ───────────────────────────────── */
static void UART2_Init(void)
{
  huart2.Instance          = USART2;
  huart2.Init.BaudRate     = 115200;
  huart2.Init.WordLength   = UART_WORDLENGTH_8B;
  huart2.Init.StopBits     = UART_STOPBITS_1;
  huart2.Init.Parity       = UART_PARITY_NONE;
  huart2.Init.Mode         = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;

  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* ─── printf retarget → UART2 ────────────────────────────────────────────── */
int __io_putchar(int ch)
{
  HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
  return ch;
}

/* ─── Error_Handler ──────────────────────────────────────────────────────── */
static void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
    /* LED toggling or debugger breakpoint */
    HAL_Delay(500);
  }
}

/* HAL assert callback */
void HAL_AssertFailed(uint8_t *file, uint32_t line)
{
  (void)file;
  (void)line;
  Error_Handler();
}
