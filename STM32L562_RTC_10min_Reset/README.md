# STM32L562 — RTC로 10분마다 Software Reset

STM32CubeMX / STM32CubeIDE 기반, **STM32L562** 에서 RTC Wakeup Timer 를 사용해
**정확히 10분(600초)마다 소프트웨어 리셋**을 거는 예제 프로젝트입니다.

---

## 1. 동작 원리

| 항목 | 내용 |
|------|------|
| 타이머 | RTC Wakeup Timer (WUT) |
| 클럭 소스 | `ck_spre` = 1 Hz (RTC 프리스케일러 출력) |
| 카운터 값 | `599` → 주기 = (599 + 1) × 1 s = **600 s = 10분** |
| 인터럽트 | `RTC_IRQn` → `HAL_RTCEx_WakeUpTimerEventCallback()` |
| 리셋 방법 | `HAL_NVIC_SystemReset()` (Cortex-M33 `AIRCR.SYSRESETREQ`) |

Wakeup Timer 는 **자동 재장전(auto-reload)** 방식이라 한 번만 설정하면 계속 반복되고,
`ck_spre` 모드에서 최대 65536 초(약 18시간)까지 설정할 수 있어 10분은 여유롭게 커버됩니다.

인터럽트 콜백에서는 플래그만 세우고 `main()` 루프에서 리셋합니다.
(ISR 안에서 바로 리셋해도 되지만, UART 로그를 끝까지 내보내고 안전하게 종료하기 위함)

리셋 후에도 **VBAT 백업 도메인(RTC/TAMP)** 은 초기화되지 않으므로
RTC 시각과 백업 레지스터에 저장한 리셋 횟수가 그대로 유지됩니다.

---

## 2. 파일 구성

```
STM32L562_RTC_10min_Reset/
├── STM32L562_RTC_10min_Reset.ioc   # CubeMX 설정 파일 (시작점)
├── Core/
│   ├── Inc/
│   │   ├── main.h                  # 사용자 설정 (주기, 핀, LSI/LSE 선택)
│   │   └── stm32l5xx_it.h
│   └── Src/
│       ├── main.c                  # RTC 초기화 + 10분 리셋 로직
│       ├── stm32l5xx_hal_msp.c     # RTC/UART MSP (클럭, NVIC, GPIO)
│       └── stm32l5xx_it.c          # RTC_IRQHandler
└── README.md
```

> HAL 드라이버(`Drivers/`), 링커 스크립트, `startup_stm32l562xx.s`, `syscalls.c` 등은
> 용량이 크므로 포함하지 않았습니다. 아래 3장대로 CubeMX/CubeIDE 에서 프로젝트를
> 생성한 뒤 위 소스 4개를 덮어쓰면 바로 빌드됩니다.

---

## 3. STM32CubeMX / CubeIDE 설정 순서

### 3-1. 프로젝트 생성
1. STM32CubeIDE → `File > New > STM32 Project`
2. Part Number 에 **STM32L562ZET6Q** (또는 사용 중인 파트) 입력 후 선택
3. 프로젝트 이름: `STM32L562_RTC_10min_Reset`, Targeted Language: **C**
4. **"Options for TrustZone" 창이 뜨면 `TrustZone: Disabled` 선택** (본 예제 기준)
   - TrustZone 을 켜면 Secure/NonSecure 두 프로젝트가 생기고, RTC 인터럽트가
     `RTC_S_IRQn` / `RTC_S_IRQHandler` 로 바뀝니다. (5장 참고)

### 3-2. 클럭 설정 (Pinout & Configuration → RCC / Clock Configuration)
- **RCC**
  - `Low Speed Clock (LSE)`: 보드에 32.768 kHz 크리스탈이 있으면 `Crystal/Ceramic Resonator`,
    없으면 그대로 **Disable** (내부 LSI 사용)
- **Clock Configuration 탭**
  - System Clock Mux: **MSI (4 MHz)** — 기본값 그대로 사용
  - **RTC Clock Mux: `LSI`** (크리스탈이 있으면 `LSE`)

### 3-3. RTC 설정 (Pinout & Configuration → Timers → RTC)
- **Mode**
  - ☑ `Activate Clock Source`
  - ☑ `Activate Calendar`
  - `WakeUp` → **`Internal WakeUp`** 선택
- **Configuration → Parameter Settings**

  | 항목 | LSI(32 kHz) | LSE(32.768 kHz) |
  |------|------|------|
  | Asynchronous Predivider value | `127` | `127` |
  | Synchronous Predivider value | `249` | `255` |
  | Wake Up Clock | `RTC_WAKEUPCLOCK_CKSPRE` | 동일 |
  | Wake Up Counter | **`599`** | 동일 |

  → `(Async+1) × (Sync+1) = 32000` 또는 `32768` 이 되어 `ck_spre = 1 Hz` 가 됩니다.

- **Configuration → NVIC Settings**
  - ☑ **`RTC global interrupt`** 활성화 (Preemption Priority 5 권장)

### 3-4. (선택) 디버그 UART / LED
- `USART1` → Mode: **Asynchronous**, Baud rate **115200** (PA9=TX, PA10=RX)
- `PA5` → **GPIO_Output** (상태 LED)
- 보드가 다르면 `Core/Inc/main.h` 상단의 핀 매크로만 수정하세요.
  - 예) NUCLEO-L552ZE-Q 는 VCP 가 **LPUART1(PG7/PG8)**, LD1 이 **PC7** 입니다.
  - 로그가 필요 없으면 `#define USE_DEBUG_UART 0` 으로 두면 됩니다.

### 3-5. 코드 생성 & 소스 반영
1. `Project Manager` → Toolchain: **STM32CubeIDE**,
   ☑ `Generate peripheral initialization as a pair of .c/.h files` (선택)
2. **GENERATE CODE (Alt+K)**
3. 생성된 프로젝트의 아래 파일을 이 저장소의 파일로 덮어쓰기
   - `Core/Inc/main.h`
   - `Core/Src/main.c`
   - `Core/Src/stm32l5xx_hal_msp.c`
   - `Core/Src/stm32l5xx_it.c`
4. 빌드(Ctrl+B) 후 다운로드/디버그 실행

> **주의:** CubeMX 에서 코드를 재생성해도 `/* USER CODE BEGIN ... END */` 블록 안의
> 코드는 보존됩니다. 본 예제의 핵심 코드는 모두 USER CODE 블록 안에 넣어두었습니다.

---

## 4. 핵심 코드

### 4-1. Wakeup Timer 설정 (`MX_RTC_Init`)
```c
/* ck_spre = 1 Hz 이므로 주기 = (WakeUpCounter + 1) 초
   -> 10분(600초) = 599 + 1 */
if (HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, (RESET_PERIOD_SEC - 1U),
                                RTC_WAKEUPCLOCK_CKSPRE, 0U) != HAL_OK)
{
  Error_Handler();
}
```

### 4-2. 인터럽트 핸들러 (`stm32l5xx_it.c`)
```c
void RTC_IRQHandler(void)
{
  HAL_RTCEx_WakeUpTimerIRQHandler(&hrtc);
}
```

### 4-3. 콜백 → 리셋 (`main.c`)
```c
void HAL_RTCEx_WakeUpTimerEventCallback(RTC_HandleTypeDef *hrtc_handle)
{
  g_reset_request = 1U;      /* ISR 에서는 플래그만 */
}

/* main loop */
if (g_reset_request)
{
  HAL_RTCEx_DeactivateWakeUpTimer(&hrtc);
  HAL_NVIC_SystemReset();    /* 소프트웨어 리셋 */
}
```

### 4-4. 주기 변경
`Core/Inc/main.h` 의 값 하나만 바꾸면 됩니다.
```c
#define RESET_PERIOD_SEC   600U   /* 10분. 예: 60U = 1분, 3600U = 1시간 */
```
> 테스트할 때는 `60U`(1분) 정도로 줄여서 확인하는 것을 권장합니다.

---

## 5. STM32L5 사용 시 주의사항

1. **RTC 인터럽트 벡터가 통합되어 있습니다.**
   STM32F4 처럼 `RTC_WKUP_IRQn` 이 따로 있지 않고, L5 는 Alarm/WakeUp/Timestamp 가
   모두 **`RTC_IRQn`(`RTC_IRQHandler`)** 하나로 들어옵니다.
   TrustZone 을 켠 Secure 프로젝트에서는 **`RTC_S_IRQn` / `RTC_S_IRQHandler`** 를 사용하세요.

2. **`__HAL_RCC_RTCAPB_CLK_ENABLE()` 필수.**
   L5 는 RTC/TAMP 레지스터 접근용 APB 클럭이 별도로 있습니다. 이게 없으면
   RTC 레지스터를 읽어도 0 만 나옵니다. (`HAL_RTC_MspInit()` 에 포함되어 있음)

3. **백업 레지스터는 TAMP 블록에 있습니다.**
   L5 는 `RTC_BKPxR` 이 아니라 `TAMP_BKPxR` 이지만, HAL API 는 동일하게
   `HAL_RTCEx_BKUPWrite()` / `HAL_RTCEx_BKUPRead()` 를 씁니다.
   쓰기 전에 `HAL_PWR_EnableBkUpAccess()` 를 호출해야 합니다.

4. **달력 재설정 금지.**
   소프트 리셋 후 `HAL_RTC_SetTime()` 을 다시 호출하면 시각이 초기화되고
   Wakeup Timer 기준도 흔들립니다. 본 예제는 `RTC_ICSR.INITS` 비트를 검사해
   **콜드 부트일 때만** 달력을 설정합니다.

5. **소프트웨어 리셋은 백업 도메인을 지우지 않습니다.**
   따라서 `RTC` 는 계속 동작하며, 리셋 원인은 `RCC->CSR` 의 `SFTRSTF` 로 확인 가능합니다.
   플래그는 읽은 뒤 `__HAL_RCC_CLEAR_RESET_FLAGS()` 로 반드시 지워야 다음 리셋 원인을
   정확히 구분할 수 있습니다.

---

## 6. 실행 결과 예시 (115200 8N1)

```
==========================================
 STM32L562 RTC 10-minute Software Reset
==========================================
 Reset cause : NRST-PIN (CSR=0x0C000000)
 Soft reset count : 0
 RTC time    : 2000-01-01 00:00:00
 Next reset in 600 s (10 min)
------------------------------------------

[RTC] 600 s elapsed -> Software reset now!

==========================================
 STM32L562 RTC 10-minute Software Reset
==========================================
 Reset cause : SOFTWARE (CSR=0x18000000)
 Soft reset count : 1
 RTC time    : 2000-01-01 00:10:00
 Next reset in 600 s (10 min)
------------------------------------------
```

---

## 7. 대안 — RTC Alarm 방식

Wakeup Timer 대신 Alarm A 로도 구현할 수 있습니다.
(현재 시각 + 10분을 계산해 알람을 걸고, 콜백에서 리셋 + 다음 알람 재설정)

```c
static void SetNextAlarm(void)
{
  RTC_AlarmTypeDef sAlarm = {0};
  RTC_TimeTypeDef  sTime  = {0};
  RTC_DateTypeDef  sDate  = {0};
  uint32_t sec;

  HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
  HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);   /* shadow 해제를 위해 필수 */

  sec = (sTime.Hours * 3600U + sTime.Minutes * 60U + sTime.Seconds
         + RESET_PERIOD_SEC) % 86400U;

  sAlarm.AlarmTime.Hours   = sec / 3600U;
  sAlarm.AlarmTime.Minutes = (sec % 3600U) / 60U;
  sAlarm.AlarmTime.Seconds = sec % 60U;
  sAlarm.AlarmMask         = RTC_ALARMMASK_DATEWEEKDAY;  /* 날짜 무시 */
  sAlarm.AlarmSubSecondMask = RTC_ALARMSUBSECONDMASK_ALL;
  sAlarm.Alarm             = RTC_ALARM_A;

  if (HAL_RTC_SetAlarm_IT(&hrtc, &sAlarm, RTC_FORMAT_BIN) != HAL_OK)
  {
    Error_Handler();
  }
}

void HAL_RTC_AlarmAEventCallback(RTC_HandleTypeDef *hrtc_handle)
{
  g_reset_request = 1U;
}
```
`stm32l5xx_it.c` 의 `RTC_IRQHandler()` 에서는 `HAL_RTC_AlarmIRQHandler(&hrtc);` 를 호출합니다.

> 단순 주기 리셋이 목적이라면 자동 재장전이 되는 **Wakeup Timer 방식(기본)** 이 더 간단합니다.
> IWDG(독립 워치독)는 최대 타임아웃이 약 32초라 10분 주기에는 사용할 수 없습니다.
