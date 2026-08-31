# STM32L562 — RTC로 24시간마다 Software Reset

STM32CubeMX / STM32CubeIDE 기반, **STM32L562** 에서 RTC Wakeup Timer 를 사용해
**정확히 24시간(86400초)마다 소프트웨어 리셋**을 거는 예제 프로젝트입니다.
주기는 `Core/Inc/main.h` 의 `RESET_PERIOD_SEC` 하나로 바꿀 수 있습니다.

---

## 1. 동작 원리

| 항목 | 내용 |
|------|------|
| 타이머 | RTC Wakeup Timer (WUT) |
| 클럭 소스 | `ck_spre` = 1 Hz (RTC 프리스케일러 출력) |
| WUCKSEL | `RTC_WAKEUPCLOCK_CK_SPRE_17BITS` (2¹⁶ 가산 모드) |
| 카운터 값 | `20863` → 주기 = (20863 + 1 + 65536) × 1 s = **86400 s = 24시간** |
| 인터럽트 | `RTC_IRQn` → `HAL_RTCEx_WakeUpTimerEventCallback()` |
| 리셋 방법 | `HAL_NVIC_SystemReset()` (Cortex-M33 `AIRCR.SYSRESETREQ`) |

Wakeup Timer 는 **자동 재장전(auto-reload)** 방식이라 한 번만 설정하면 계속 반복됩니다.

> ### ⚠ 24시간은 16비트 카운터만으로는 불가능합니다
> WUT 카운터는 16비트라서 `ck_spre`(1 Hz) 기준 **최대 65536초 ≈ 18.2시간**입니다.
> 86400초는 이 범위를 넘으므로, `WUCKSEL[2:1] = 11` (**CK_SPRE_17BITS**) 모드를 써서
> 카운터에 **2¹⁶(65536)을 더하는** 방식으로 설정해야 합니다.
> 이 모드의 주기는 `(WUT + 1 + 65536)초` 이고, 최대 **131072초 ≈ 36.4시간** 까지 가능합니다.
>
> 본 프로젝트는 `main.h` 에서 `RESET_PERIOD_SEC` 값을 보고
> 16비트/17비트 모드와 카운터 값을 **컴파일 타임에 자동으로 계산**합니다.

인터럽트 콜백에서는 플래그만 세우고 `main()` 루프에서 리셋합니다.
(ISR 안에서 바로 리셋해도 되지만, UART 로그를 끝까지 내보내고 안전하게 종료하기 위함)

리셋 후에도 **VBAT 백업 도메인(RTC/TAMP)** 은 초기화되지 않으므로
RTC 시각과 백업 레지스터에 저장한 리셋 횟수가 그대로 유지됩니다.

---

## 2. 파일 구성

```
STM32L562_RTC_Periodic_Reset/
├── STM32L562_RTC_Periodic_Reset.ioc   # CubeMX 설정 파일 (시작점)
├── Core/
│   ├── Inc/
│   │   ├── main.h                  # 사용자 설정 (주기, 핀, LSI/LSE 선택)
│   │   └── stm32l5xx_it.h
│   └── Src/
│       ├── main.c                  # RTC 초기화 + 주기적 리셋 로직
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
3. 프로젝트 이름: `STM32L562_RTC_Periodic_Reset`, Targeted Language: **C**
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
  | Wake Up Clock | `RTC_WAKEUPCLOCK_CK_SPRE_17BITS` | 동일 |
  | Wake Up Counter | **`20863`** | 동일 |

  → `(Async+1) × (Sync+1) = 32000` 또는 `32768` 이 되어 `ck_spre = 1 Hz` 가 됩니다.
  → 카운터 `20863` + 1 + 65536 = **86400초 = 24시간**
  → CubeMX 버전에 따라 Wake Up Clock 목록에 17BITS 항목이 없을 수 있는데,
    그때는 CubeMX 값은 그대로 두고 코드(`main.h`)의 자동 계산 값이 적용되므로 문제없습니다.

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
`main.h` 에서 주기에 맞는 모드/카운터를 자동으로 고릅니다.
```c
#if   (RESET_PERIOD_SEC <= 65536U)          /* 최대 약 18.2시간 */
  #define WUT_CLOCK_SEL   RTC_WAKEUPCLOCK_CK_SPRE_16BITS
  #define WUT_COUNTER     (RESET_PERIOD_SEC - 1U)
#elif (RESET_PERIOD_SEC <= 131072U)         /* 최대 약 36.4시간 */
  #define WUT_CLOCK_SEL   RTC_WAKEUPCLOCK_CK_SPRE_17BITS
  #define WUT_COUNTER     (RESET_PERIOD_SEC - 65536U - 1U)
#else
  #error "RESET_PERIOD_SEC 가 너무 큽니다(최대 131072초)."
#endif
```
`main.c` 에서는 계산된 값을 그대로 넘기기만 합니다.
```c
/* 24시간 -> CK_SPRE_17BITS, WUT = 86400 - 65536 - 1 = 20863 */
if (HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, WUT_COUNTER,
                                WUT_CLOCK_SEL, 0U) != HAL_OK)
{
  Error_Handler();
}
```
> HAL 버전이 오래되어 `HAL_RTCEx_SetWakeUpTimer_IT()` 이 3개 인자만 받는다면
> 마지막 `0U`(WakeUpAutoClr)를 빼면 됩니다.

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
`Core/Inc/main.h` 의 값 하나만 바꾸면 됩니다. 나머지는 전부 자동 계산됩니다.
```c
#define RESET_PERIOD_SEC   (24U * 3600U)   /* 86400초 = 24시간 */
```
| 원하는 주기 | 설정 값 | 자동 선택되는 모드 / 카운터 |
|------|------|------|
| 1분 | `60U` | 16BITS / 59 |
| 10분 | `600U` | 16BITS / 599 |
| 1시간 | `3600U` | 16BITS / 3599 |
| 12시간 | `(12U * 3600U)` | 16BITS / 43199 |
| **24시간** | `(24U * 3600U)` | **17BITS / 20863** |
| 36시간 | `(36U * 3600U)` | 17BITS / 64063 |
| 36.4시간 초과 | — | ❌ `#error` (Alarm 방식 사용, 7장 참고) |

> 24시간은 실제로 확인하는 데 하루가 걸리므로, 동작 검증은 `60U`(1분) 로 줄여서 하고
> 확인이 끝난 뒤 `(24U * 3600U)` 로 되돌리는 것을 권장합니다.

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

6. **24시간 주기에서는 클럭 정확도가 중요합니다.**
   내부 **LSI 는 오차가 ±5% 수준**이라 24시간 기준 **최대 ±72분**까지 벌어질 수 있습니다.
   "매일 정해진 시각에" 리셋해야 한다면 반드시 외부 32.768 kHz 크리스탈(**LSE**) 을 쓰고
   `main.h` 의 `RTC_CLOCK_LSE` 를 `1` 로 설정하세요 (LSE 는 보통 ±20 ppm ≒ 하루 ±2초).
   LSI 를 쓴 채 1시간을 넘는 주기를 설정하면 빌드 시 `#warning` 으로 알려줍니다.

7. **주기를 바꿔도 리셋 시점은 "부팅 시점 기준"입니다.**
   Wakeup Timer 는 리셋 직후 다시 0부터 세므로, 전원을 켠 시각으로부터 24시간마다
   리셋됩니다. **매일 새벽 3시처럼 고정된 벽시계 시각**에 리셋하려면 7장의 Alarm 방식을 쓰세요.

---

## 6. 실행 결과 예시 (115200 8N1)

```
==========================================
 STM32L562 RTC Periodic Software Reset
==========================================
 Reset cause : NRST-PIN (CSR=0x0C000000)
 Soft reset count : 0
 RTC time    : 2000-01-01 00:00:00
 Next reset in 86400 s (24h 00m)
------------------------------------------

... (24시간 경과) ...

[RTC] 86400 s elapsed -> Software reset now!

==========================================
 STM32L562 RTC Periodic Software Reset
==========================================
 Reset cause : SOFTWARE (CSR=0x18000000)
 Soft reset count : 1
 RTC time    : 2000-01-02 00:00:00
 Next reset in 86400 s (24h 00m)
------------------------------------------
```


---

## 7. 대안 — RTC Alarm 방식 (매일 정해진 시각에 리셋)

"부팅 후 24시간"이 아니라 **매일 같은 벽시계 시각**(예: 새벽 3시)에 리셋하고 싶다면
Wakeup Timer 대신 **Alarm A** 가 더 적합합니다.
`AlarmMask` 에 `RTC_ALARMMASK_DATEWEEKDAY` 를 주면 날짜를 무시하고
**하루에 한 번, 지정한 시:분:초에** 알람이 발생합니다 — 즉 24시간 주기가 그대로 나옵니다.

```c
/* 매일 03:00:00 에 리셋 */
#define DAILY_RESET_HOUR    3U
#define DAILY_RESET_MINUTE  0U

static void SetDailyAlarm(void)
{
  RTC_AlarmTypeDef sAlarm = {0};

  sAlarm.AlarmTime.Hours          = DAILY_RESET_HOUR;
  sAlarm.AlarmTime.Minutes        = DAILY_RESET_MINUTE;
  sAlarm.AlarmTime.Seconds        = 0U;
  sAlarm.AlarmTime.SubSeconds     = 0U;
  sAlarm.AlarmTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
  sAlarm.AlarmTime.StoreOperation = RTC_STOREOPERATION_RESET;
  sAlarm.AlarmMask                = RTC_ALARMMASK_DATEWEEKDAY;  /* 날짜 무시 = 매일 */
  sAlarm.AlarmSubSecondMask       = RTC_ALARMSUBSECONDMASK_ALL;
  sAlarm.Alarm                    = RTC_ALARM_A;

  if (HAL_RTC_SetAlarm_IT(&hrtc, &sAlarm, RTC_FORMAT_BIN) != HAL_OK)
  {
    Error_Handler();
  }
}

void HAL_RTC_AlarmAEventCallback(RTC_HandleTypeDef *hrtc_handle)
{
  g_reset_request = 1U;      /* 알람은 자동 반복되므로 재설정 불필요 */
}
```
- `stm32l5xx_it.c` 의 `RTC_IRQHandler()` 에서 `HAL_RTC_AlarmIRQHandler(&hrtc);` 를 호출합니다.
- 이 방식은 **RTC 달력 시각이 실제 시각과 맞아야** 의미가 있으므로,
  콜드 부트 시 `HAL_RTC_SetTime()` 으로 현재 시각을 넣어주어야 합니다.

> **정리**
> - 부팅 시점 기준 24시간마다 → **Wakeup Timer** (본 예제 기본, 설정 한 줄)
> - 매일 고정된 시각에 → **Alarm A + DATEWEEKDAY 마스크**
> - IWDG(독립 워치독)는 최대 타임아웃이 약 32초라 장주기 리셋에는 사용할 수 없습니다.
