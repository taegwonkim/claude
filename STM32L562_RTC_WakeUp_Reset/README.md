# ① STM32L562 — RTC Wakeup Timer 로 주기적 Software Reset

**부팅 시점으로부터 N초마다** 소프트웨어 리셋을 거는 프로젝트입니다. (기본 24시간)

> 벽시계 시각 기준(예: 매일 새벽 3시)으로 리셋하려면 → **[`../STM32L562_RTC_AlarmA_Reset`](../STM32L562_RTC_AlarmA_Reset)**

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
**달력 시각과 무관하게** 동작하므로 시각 동기화가 필요 없는 것이 이 방식의 장점입니다.

> ### ⚠ 24시간은 16비트 카운터만으로는 불가능합니다
> WUT 카운터는 16비트라서 `ck_spre`(1 Hz) 기준 **최대 65536초 ≈ 18.2시간**입니다.
> 86400초는 이 범위를 넘으므로 `WUCKSEL[2:1] = 11` (**CK_SPRE_17BITS**) 모드로
> 카운터에 **2¹⁶(65536)을 더해야** 합니다.
> 이 모드의 주기는 `(WUT + 1 + 65536)초`, 최대 **131072초 ≈ 36.4시간** 입니다.
>
> `main.h` 가 `RESET_PERIOD_SEC` 값을 보고 16/17비트 모드와 카운터를
> **컴파일 타임에 자동 계산**합니다.

인터럽트 콜백에서는 플래그만 세우고 `main()` 루프에서 리셋합니다.
리셋 후에도 **VBAT 백업 도메인(RTC/TAMP)** 은 초기화되지 않으므로
RTC 시각과 백업 레지스터의 리셋 횟수가 그대로 유지됩니다.

---

## 2. 파일 구성

```
STM32L562_RTC_WakeUp_Reset/
├── STM32L562_RTC_WakeUp_Reset.ioc   # CubeMX 설정 파일 (시작점)
├── Core/
│   ├── Inc/
│   │   ├── main.h                   # 주기, 핀, LSI/LSE 선택
│   │   └── stm32l5xx_it.h
│   └── Src/
│       ├── main.c                   # RTC 초기화 + Wakeup Timer 리셋 로직
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
3. 프로젝트 이름: `STM32L562_RTC_WakeUp_Reset`, Targeted Language: **C**
4. **"Options for TrustZone" 창이 뜨면 `TrustZone: Disabled` 선택**
   - TrustZone 을 켜면 Secure/NonSecure 두 프로젝트가 생기고 RTC 인터럽트가
     `RTC_S_IRQn` / `RTC_S_IRQHandler` 로 바뀝니다.

### 3-2. 클럭 설정 (RCC / Clock Configuration)
- **RCC** → `Low Speed Clock (LSE)`: 보드에 32.768 kHz 크리스탈이 있으면
  `Crystal/Ceramic Resonator`, 없으면 **Disable** (내부 LSI 사용)
- **Clock Configuration 탭**
  - System Clock Mux: **MSI (4 MHz)** — 기본값 그대로
  - **RTC Clock Mux: `LSI`** (크리스탈이 있으면 `LSE`)

### 3-3. RTC 설정 (Timers → RTC)
- **Mode**
  - ☑ `Activate Clock Source`
  - ☑ `Activate Calendar`
  - `WakeUp` → **`Internal WakeUp`**
- **Configuration → Parameter Settings**

  | 항목 | LSI(32 kHz) | LSE(32.768 kHz) |
  |------|------|------|
  | Asynchronous Predivider value | `127` | `127` |
  | Synchronous Predivider value | `249` | `255` |
  | Wake Up Clock | `RTC_WAKEUPCLOCK_CK_SPRE_17BITS` | 동일 |
  | Wake Up Counter | **`20863`** | 동일 |

  → `(Async+1) × (Sync+1) = 32000` 또는 `32768` 이 되어 `ck_spre = 1 Hz`
  → `20863 + 1 + 65536 = 86400초 = 24시간`
  → CubeMX 버전에 따라 17BITS 항목이 목록에 없을 수 있는데, 코드(`main.h`)의
    자동 계산 값이 최종 적용되므로 문제없습니다.

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

### 4-1. 주기 → 모드/카운터 자동 계산 (`main.h`)
```c
#define RESET_PERIOD_SEC   (24U * 3600U)   /* 86400초 = 24시간 */

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

### 4-2. Wakeup Timer 설정 (`MX_RTC_Init`)
```c
/* 24시간 -> CK_SPRE_17BITS, WUT = 86400 - 65536 - 1 = 20863 */
if (HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, WUT_COUNTER,
                                WUT_CLOCK_SEL, 0U) != HAL_OK)
{
  Error_Handler();
}
```
> HAL 버전이 오래되어 인자가 3개뿐이라면 마지막 `0U`(WakeUpAutoClr)를 빼세요.

### 4-3. 인터럽트 핸들러 (`stm32l5xx_it.c`)
```c
void RTC_IRQHandler(void)
{
  HAL_RTCEx_WakeUpTimerIRQHandler(&hrtc);
}
```

### 4-4. 콜백 → 리셋 (`main.c`)
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

### 4-5. 주기 변경
`Core/Inc/main.h` 의 값 하나만 바꾸면 나머지는 자동 계산됩니다.

| 원하는 주기 | 설정 값 | 자동 선택되는 모드 / 카운터 |
|------|------|------|
| 1분 | `60U` | 16BITS / 59 |
| 10분 | `600U` | 16BITS / 599 |
| 1시간 | `3600U` | 16BITS / 3599 |
| 12시간 | `(12U * 3600U)` | 16BITS / 43199 |
| **24시간** | `(24U * 3600U)` | **17BITS / 20863** |
| 36시간 | `(36U * 3600U)` | 17BITS / 64063 |
| 36.4시간 초과 | — | ❌ `#error` |

> 24시간은 검증에 하루가 걸리므로, 동작 확인은 `60U`(1분) 로 줄여서 하고
> 확인 후 `(24U * 3600U)` 로 되돌리는 것을 권장합니다.

---

## 5. STM32L5 사용 시 주의사항

1. **RTC 인터럽트 벡터가 통합되어 있습니다.**
   STM32F4 처럼 `RTC_WKUP_IRQn` 이 따로 없고, L5 는 Alarm/WakeUp/Timestamp 가
   모두 **`RTC_IRQn`(`RTC_IRQHandler`)** 하나로 들어옵니다.
   TrustZone Secure 프로젝트에서는 **`RTC_S_IRQn` / `RTC_S_IRQHandler`** 를 쓰세요.

2. **`__HAL_RCC_RTCAPB_CLK_ENABLE()` 필수.**
   L5 는 RTC/TAMP 레지스터 접근용 APB 클럭이 별도입니다. 없으면 RTC 레지스터를
   읽어도 0 만 나옵니다. (`HAL_RTC_MspInit()` 에 포함되어 있음)

3. **백업 레지스터는 TAMP 블록에 있습니다.**
   L5 는 `TAMP_BKPxR` 이지만 HAL API 는 동일하게
   `HAL_RTCEx_BKUPWrite()` / `HAL_RTCEx_BKUPRead()` 를 씁니다.
   쓰기 전에 `HAL_PWR_EnableBkUpAccess()` 를 호출해야 합니다.

4. **달력 재설정 금지.**
   소프트 리셋 후 `HAL_RTC_SetTime()` 을 다시 호출하면 시각이 초기화됩니다.
   `RTC_ICSR.INITS` 비트를 검사해 **콜드 부트일 때만** 달력을 설정합니다.

5. **리셋 원인은 `RCC->CSR` 의 `SFTRSTF` 로 확인합니다.**
   읽은 뒤 `__HAL_RCC_CLEAR_RESET_FLAGS()` 로 지워야 다음 리셋 원인을 구분할 수 있습니다.

6. **LSI 는 오차가 ±5% 입니다.** 24시간 기준 최대 ±72분까지 벌어집니다.
   정확도가 필요하면 LSE 를 쓰고 `RTC_CLOCK_LSE` 를 `1` 로 설정하세요.
   (LSI + 1시간 초과 주기 조합이면 빌드 시 `#warning` 이 뜹니다)

---

## 6. 실행 결과 예시 (115200 8N1)

```
==========================================
 STM32L562 RTC WakeUp Timer Reset (24h)
==========================================
 Reset cause : NRST-PIN (CSR=0x0C000000)
 Soft reset count : 0
 RTC time    : 2000-01-01 00:00:00
 Trigger     : RTC WakeUp Timer
 Next reset in 86400 s (24h 00m)
------------------------------------------

... (24시간 경과) ...

[RTC] 86400 s elapsed -> Software reset now!

==========================================
 STM32L562 RTC WakeUp Timer Reset (24h)
==========================================
 Reset cause : SOFTWARE (CSR=0x18000000)
 Soft reset count : 1
 RTC time    : 2000-01-02 00:00:00
 Trigger     : RTC WakeUp Timer
 Next reset in 86400 s (24h 00m)
------------------------------------------
```

RTC 는 백업 도메인에서 계속 동작하므로, 부팅할 때마다 찍히는 `RTC time` 이
정확히 24시간씩 증가하는지로 주기를 검증할 수 있습니다.

> IWDG(독립 워치독)는 최대 타임아웃이 약 32초라 장주기 리셋에는 쓸 수 없습니다.
