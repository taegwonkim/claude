# ② STM32L562 — RTC Alarm A 로 매일 정해진 시각에 Software Reset

**벽시계 시각 기준**으로 소프트웨어 리셋을 거는 프로젝트입니다. (기본 매일 03:00:00)

> 부팅 시점으로부터 N시간마다 리셋하려면 → **[`../STM32L562_RTC_WakeUp_Reset`](../STM32L562_RTC_WakeUp_Reset)**

---

## 1. 동작 원리

| 항목 | 내용 |
|------|------|
| 트리거 | RTC Alarm A |
| 핵심 설정 | `AlarmMask = RTC_ALARMMASK_DATEWEEKDAY` |
| 주기 | 날짜를 비교에서 제외 → **매일 1회 = 24시간** |
| 인터럽트 | `RTC_IRQn` → `HAL_RTC_AlarmAEventCallback()` |
| 리셋 방법 | `HAL_NVIC_SystemReset()` (Cortex-M33 `AIRCR.SYSRESETREQ`) |

### 마스크 하나로 24시간 주기가 만들어집니다

RTC 알람은 `날짜 / 시 / 분 / 초` 를 달력과 비교해 일치할 때 발생합니다.
여기서 **날짜를 마스킹**하면 남은 `시:분:초` 만 비교하므로,
지정한 시각이 될 때마다 — 즉 **하루에 한 번** — 알람이 발생합니다.

```c
sAlarm.AlarmMask = RTC_ALARMMASK_DATEWEEKDAY;   /* 날짜 무시 = 매일 반복 */
```

하드웨어가 알아서 반복하므로 **콜백에서 재장전할 필요가 없습니다.**

인터럽트 콜백에서는 플래그만 세우고 `main()` 루프에서 리셋합니다.
리셋 후에도 **VBAT 백업 도메인(RTC/TAMP)** 은 초기화되지 않으므로
RTC 시각과 백업 레지스터의 리셋 횟수가 그대로 유지됩니다.

### ⚠ 이 방식은 RTC 달력이 실제 시각과 맞아야 합니다

Wakeup Timer 와 달리 Alarm 은 벽시계 시각을 보므로, 달력이 엉뚱하면
"매일 새벽 3시"가 성립하지 않습니다. 이를 위해 `SetInitialDateTime()` 이
**컴파일 시각(`__DATE__` / `__TIME__`)** 을 파싱해 콜드 부트 시 달력을 채웁니다
(요일은 Sakamoto 알고리즘으로 계산).

```c
#define RTC_INIT_FROM_BUILD_TIME  1U   /* 0 이면 2000-01-01 00:00:00 으로 시작 */
```

> 빌드 시각과 실제 플래싱 시각의 차이만큼 오차가 남습니다.
> 정확한 시각이 필요하면 GPS / NTP / 호스트 통신으로 받은 값을
> `SetInitialDateTime()` 에 넣으세요.
> 소프트 리셋 후에는 `RTC_ICSR.INITS` 검사로 달력을 **다시 쓰지 않으므로**
> 한 번 맞춰둔 시각이 계속 유지됩니다.

---

## 1-1. 저전력 모드에 진입하지 않습니다

RTC Alarm 은 저전력 모드에서 MCU 를 깨우는 용도로도 쓰이지만, 저전력 모드에
**들어가야 하는 것은 아닙니다.** 알람은 RTC 달력과 알람 레지스터를 비교해
인터럽트를 걸 뿐이고, MCU 가 풀스피드로 돌든 STOP 모드에 있든 무관합니다.

이 프로젝트에는 저전력 진입 코드가 **하나도 없습니다.**

| 사용하지 않는 API | 상태 |
|---|---|
| `HAL_PWR_EnterSLEEPMode()` | ❌ 없음 |
| `HAL_PWR_EnterSTOPMode()` / `HAL_PWREx_EnterSTOP0/1/2Mode()` | ❌ 없음 |
| `HAL_PWR_EnterSTANDBYMode()` / `HAL_PWREx_EnterSHUTDOWNMode()` | ❌ 없음 |
| `__WFI()` / `__WFE()` | ❌ 없음 |

`main()` 의 `while(1)` 은 `HAL_GetTick()` 을 폴링하며 LED 를 토글하는
**완전 활성 상태의 busy loop** 입니다. MCU 는 리셋 시점까지 계속 동작하고,
그 사이 원하는 애플리케이션 코드를 `/* USER CODE BEGIN 3 */` 에 넣으면 됩니다.

> 반대로 저전력이 필요해지면, 이 루프 안에 `HAL_PWREx_EnterSTOP2Mode()` 한 줄만
> 넣으면 그대로 저전력 버전이 됩니다. RTC 는 STOP 모드에서도 백업 도메인에서
> 계속 돌기 때문에 나머지 코드는 손댈 필요가 없습니다.

## 1-2. 살아있음(heartbeat) 로그

계속 동작 중인지 확인하기 쉽도록 **1분마다 uptime 과 리셋까지 남은 시간**을
UART 로 출력합니다.

```c
#define USE_HEARTBEAT_LOG     1U    /* 0 이면 출력 안 함 */
#define HEARTBEAT_PERIOD_SEC  60U   /* 출력 주기 [초] */
```

```
[ALIVE] uptime 00:01:00 | RTC 2026-09-01 14:21:11 | reset in 45529 s (12h 38m)
[ALIVE] uptime 00:02:00 | RTC 2026-09-01 14:22:11 | reset in 45469 s (12h 37m)
```

LED 하트비트(500ms 토글)와 함께, MCU 가 잠들지 않고 도는지 육안/로그 양쪽으로
확인할 수 있습니다.

---

## 2. 파일 구성

```
STM32L562_RTC_AlarmA_Reset/
├── STM32L562_RTC_AlarmA_Reset.ioc   # CubeMX 설정 파일 (시작점)
├── Core/
│   ├── Inc/
│   │   ├── main.h                   # 리셋 시각, 핀, LSI/LSE 선택
│   │   └── stm32l5xx_it.h
│   └── Src/
│       ├── main.c                   # RTC 초기화 + Alarm A 리셋 로직
│       ├── stm32l5xx_hal_msp.c      # RTC/UART MSP (클럭, NVIC, GPIO)
│       └── stm32l5xx_it.c           # RTC_IRQHandler
└── README.md
```

> HAL 드라이버(`Drivers/`), 링커 스크립트, `startup_stm32l562xx.s`, `syscalls.c` 는
> 용량이 커서 포함하지 않았습니다. 3장대로 CubeMX/CubeIDE 에서 프로젝트를 만든 뒤
> 위 소스 4개를 덮어쓰면 바로 빌드됩니다.

---

## 3. STM32CubeMX / CubeIDE 설정 순서

### 3-1. 프로젝트 생성
1. STM32CubeIDE → `File > New > STM32 Project`
2. Part Number 에 **STM32L562ZET6Q** (또는 사용 중인 파트) 입력 후 선택
3. 프로젝트 이름: `STM32L562_RTC_AlarmA_Reset`, Targeted Language: **C**
4. **"Options for TrustZone" 창이 뜨면 `TrustZone: Disabled` 선택**

### 3-2. 클럭 설정 (RCC / Clock Configuration)
- **RCC** → `Low Speed Clock (LSE)`: **`Crystal/Ceramic Resonator` 강력 권장**
  (아래 5장 6번 참고 — 벽시계 시각을 쓰는 방식이라 LSI 오차가 치명적입니다)
- **Clock Configuration 탭**
  - System Clock Mux: **MSI (4 MHz)** — 기본값 그대로
  - **RTC Clock Mux: `LSE`** (크리스탈이 없으면 `LSI`)

### 3-3. RTC 설정 (Timers → RTC)
- **Mode**
  - ☑ `Activate Clock Source`
  - ☑ `Activate Calendar`
  - `Alarm A` → **`Internal Alarm A`**
- **Configuration → Parameter Settings**

  | 항목 | LSI(32 kHz) | LSE(32.768 kHz) |
  |------|------|------|
  | Asynchronous Predivider value | `127` | `127` |
  | Synchronous Predivider value | `249` | `255` |

  → `(Async+1) × (Sync+1) = 32000` 또는 `32768` 이 되어 `ck_spre = 1 Hz`
  → 알람 시각/마스크는 코드(`RTC_SetResetAlarm()`)에서 설정하므로
    CubeMX 의 Alarm 파라미터 값은 무관합니다.

- **Configuration → NVIC Settings** → ☑ **`RTC global interrupt`** (Priority 5 권장)

### 3-4. (선택) 디버그 UART / LED
- `USART1` → Mode **Asynchronous**, Baud rate **115200** (PA9=TX, PA10=RX)
- `PA5` → **GPIO_Output** (상태 LED)
- 보드가 다르면 `Core/Inc/main.h` 상단의 핀 매크로만 수정하세요.
  - 예) NUCLEO-L552ZE-Q 는 VCP 가 **LPUART1(PG7/PG8)**, LD1 이 **PC7**
  - 로그가 필요 없으면 `#define USE_DEBUG_UART 0`

### 3-5. 코드 생성 & 소스 반영
1. `Project Manager` → Toolchain **STM32CubeIDE** → **GENERATE CODE (Alt+K)**
2. 생성된 프로젝트의 아래 파일을 이 폴더의 파일로 덮어쓰기
   - `Core/Inc/main.h`, `Core/Src/main.c`,
     `Core/Src/stm32l5xx_hal_msp.c`, `Core/Src/stm32l5xx_it.c`
3. 빌드(Ctrl+B) 후 다운로드/디버그 실행

> 핵심 코드는 모두 `/* USER CODE BEGIN ... END */` 블록 안에 있어서
> CubeMX 에서 재생성해도 보존됩니다.

---

## 4. 핵심 코드

### 4-1. 리셋 시각 설정 (`main.h`)
```c
#define ALARM_MODE            ALARM_MODE_DAILY_FIXED   /* 또는 ALARM_MODE_RELATIVE */

/* ALARM_MODE_DAILY_FIXED 에서 리셋할 시각 (24시간 표기) */
#define ALARM_RESET_HOUR      3U
#define ALARM_RESET_MINUTE    0U
#define ALARM_RESET_SECOND    0U

/* ALARM_MODE_RELATIVE 에서 사용할 주기 [초] (86400초 이하) */
#define RESET_PERIOD_SEC      (24U * 3600U)
```

- **`ALARM_MODE_DAILY_FIXED`** (기본) — 매일 `03:00:00` 에 리셋.
  주기는 항상 24시간 고정이며 재장전이 필요 없습니다.
- **`ALARM_MODE_RELATIVE`** — `(현재 시각 + RESET_PERIOD_SEC)` 에 알람을 겁니다.
  날짜를 마스킹하므로 `RESET_PERIOD_SEC` 는 **86400초(24h) 이하만** 가능하고,
  넘기면 빌드 시 `#error` 로 막힙니다.

### 4-2. 알람 설정 (`main.c`)
```c
static void RTC_SetResetAlarm(void)
{
  RTC_AlarmTypeDef sAlarm = {0};

#if (ALARM_MODE == ALARM_MODE_DAILY_FIXED)
  sAlarm.AlarmTime.Hours   = ALARM_RESET_HOUR;
  sAlarm.AlarmTime.Minutes = ALARM_RESET_MINUTE;
  sAlarm.AlarmTime.Seconds = ALARM_RESET_SECOND;
#else
  /* 현재 시각 + RESET_PERIOD_SEC (GetTime -> GetDate 순서 필수) */
  HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
  HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);
  now_sec    = sTime.Hours * 3600U + sTime.Minutes * 60U + sTime.Seconds;
  target_sec = (now_sec + RESET_PERIOD_SEC) % 86400U;
  sAlarm.AlarmTime.Hours   = target_sec / 3600U;
  sAlarm.AlarmTime.Minutes = (target_sec % 3600U) / 60U;
  sAlarm.AlarmTime.Seconds = target_sec % 60U;
#endif

  sAlarm.AlarmTime.SubSeconds     = 0U;
  sAlarm.AlarmTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
  sAlarm.AlarmTime.StoreOperation = RTC_STOREOPERATION_RESET;
  sAlarm.AlarmMask           = RTC_ALARMMASK_DATEWEEKDAY;   /* 날짜 무시 = 매일 */
  sAlarm.AlarmSubSecondMask  = RTC_ALARMSUBSECONDMASK_ALL;
  sAlarm.AlarmDateWeekDaySel = RTC_ALARMDATEWEEKDAYSEL_DATE;
  sAlarm.AlarmDateWeekDay    = 1U;                          /* 마스킹되어 무의미 */
  sAlarm.Alarm               = RTC_ALARM_A;

  HAL_RTC_DeactivateAlarm(&hrtc, RTC_ALARM_A);   /* 재설정 전 해제 */
  if (HAL_RTC_SetAlarm_IT(&hrtc, &sAlarm, RTC_FORMAT_BIN) != HAL_OK)
  {
    Error_Handler();
  }
}
```

### 4-3. 인터럽트 핸들러 (`stm32l5xx_it.c`)
```c
void RTC_IRQHandler(void)
{
  HAL_RTC_AlarmIRQHandler(&hrtc);
}
```

### 4-4. 콜백 → 리셋 (`main.c`)
```c
void HAL_RTC_AlarmAEventCallback(RTC_HandleTypeDef *hrtc_handle)
{
  g_reset_request = 1U;      /* ISR 에서는 플래그만 */
}

/* main loop */
if (g_reset_request)
{
  HAL_RTC_DeactivateAlarm(&hrtc, RTC_ALARM_A);
  HAL_NVIC_SystemReset();    /* 소프트웨어 리셋 */
}
```

### 4-5. 동작 확인 요령
매일 새벽 3시를 기다릴 수 없으므로, 검증할 때는 `ALARM_RESET_HOUR/MINUTE/SECOND`
를 **지금 시각의 1~2분 뒤**로 잡거나, `ALARM_MODE_RELATIVE` + `RESET_PERIOD_SEC 60U`
로 바꿔서 1분마다 리셋되는지 확인한 뒤 원래 값으로 되돌리세요.

---

## 5. STM32L5 사용 시 주의사항

1. **RTC 인터럽트 벡터가 통합되어 있습니다.**
   STM32F4 처럼 `RTC_Alarm_IRQn` 이 따로 없고, L5 는 Alarm/WakeUp/Timestamp 가
   모두 **`RTC_IRQn`(`RTC_IRQHandler`)** 하나로 들어옵니다.
   TrustZone Secure 프로젝트에서는 **`RTC_S_IRQn` / `RTC_S_IRQHandler`** 를 쓰세요.

2. **`__HAL_RCC_RTCAPB_CLK_ENABLE()` 필수.**
   L5 는 RTC/TAMP 레지스터 접근용 APB 클럭이 별도입니다. 없으면 RTC 레지스터를
   읽어도 0 만 나옵니다. (`HAL_RTC_MspInit()` 에 포함되어 있음)

3. **`HAL_RTC_GetTime()` 뒤에는 반드시 `HAL_RTC_GetDate()` 를 호출하세요.**
   GetTime 이 shadow register 를 잠그고, GetDate 가 잠금을 해제합니다.
   빠뜨리면 다음 읽기부터 시각이 갱신되지 않습니다.

4. **백업 레지스터는 TAMP 블록에 있습니다.**
   L5 는 `TAMP_BKPxR` 이지만 HAL API 는 동일하게
   `HAL_RTCEx_BKUPWrite()` / `HAL_RTCEx_BKUPRead()` 를 씁니다.
   쓰기 전에 `HAL_PWR_EnableBkUpAccess()` 를 호출해야 합니다.

5. **달력 재설정 금지.**
   소프트 리셋 후 `HAL_RTC_SetTime()` 을 다시 호출하면 애써 맞춘 시각이
   초기화됩니다. `RTC_ICSR.INITS` 비트를 검사해 **콜드 부트일 때만** 설정합니다.

6. **LSI 를 쓰면 벽시계 시각이 하루 최대 ±72분 틀어집니다.**
   LSI 오차는 ±5% 수준이라 "매일 03:00" 이 며칠 만에 무의미해집니다.
   이 방식에서는 **LSE(외부 32.768 kHz 크리스탈, 보통 ±20 ppm ≒ 하루 ±2초)**
   를 쓰고 `RTC_CLOCK_LSE` 를 `1` 로 설정하세요.
   (LSI 로 설정되어 있으면 빌드 시 `#warning` 이 뜹니다)

7. **리셋 원인은 `RCC->CSR` 의 `SFTRSTF` 로 확인합니다.**
   읽은 뒤 `__HAL_RCC_CLEAR_RESET_FLAGS()` 로 지워야 다음 리셋 원인을 구분할 수 있습니다.

---

## 6. 실행 결과 예시 (115200 8N1)

```
==========================================
 STM32L562 RTC Alarm A Reset (daily)
==========================================
 Reset cause : NRST-PIN (CSR=0x0C000000)
 Soft reset count : 0
 RTC time    : 2026-09-01 14:20:11
 Trigger     : RTC Alarm A (daily fixed)
 Next reset at 03:00:00 every day
------------------------------------------
[ALIVE] uptime 00:01:00 | RTC 2026-09-01 14:21:11 | reset in 45529 s (12h 38m)
[ALIVE] uptime 00:02:00 | RTC 2026-09-01 14:22:11 | reset in 45469 s (12h 37m)

... (1분마다 계속 출력, 다음 03:00:00 까지) ...

[ALIVE] uptime 12:38:49 | RTC 2026-09-02 02:59:00 | reset in 60 s (0h 01m)
[RTC] Alarm A fired -> Software reset now!

==========================================
 STM32L562 RTC Alarm A Reset (daily)
==========================================
 Reset cause : SOFTWARE (CSR=0x18000000)
 Soft reset count : 1
 RTC time    : 2026-09-02 03:00:00
 Trigger     : RTC Alarm A (daily fixed)
 Next reset at 03:00:00 every day
------------------------------------------
```

> IWDG(독립 워치독)는 최대 타임아웃이 약 32초라 장주기 리셋에는 쓸 수 없습니다.
