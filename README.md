# STM32L562 — RTC Wakeup Timer 로 주기적 Software Reset

STM32CubeMX / STM32CubeIDE 기반, **STM32L562** 에서 RTC Wakeup Timer 를 사용해
**부팅 시점으로부터 일정 시간마다 소프트웨어 리셋**을 거는 프로젝트입니다.

- RTC 클럭 : **내부 LSI(~32 kHz)** — 외부 크리스탈 불필요
- 리셋 주기 : **1분 ~ 30시간** 설정 가능 (기본값 **30시간**)
- 저전력 모드에 진입하지 않고 **계속 동작**하다가 시간이 되면 리셋

---

## 1. 동작 원리

| 항목 | 내용 |
|------|------|
| 타이머 | RTC Wakeup Timer (WUT) |
| RTC 클럭 | 내부 **LSI ~32 kHz** (`AsynchPrediv=127`, `SynchPrediv=249`) |
| 카운트 클럭 | `ck_spre` = 1 Hz |
| 기본 설정 | `RTC_WAKEUPCLOCK_CK_SPRE_17BITS`, 카운터 `42463` → **108000 s = 30시간** |
| 인터럽트 | `RTC_IRQn` → `HAL_RTCEx_WakeUpTimerEventCallback()` |
| 리셋 방법 | `HAL_NVIC_SystemReset()` (Cortex-M33 `AIRCR.SYSRESETREQ`) |

Wakeup Timer 는 **자동 재장전(auto-reload)** 방식이라 한 번만 설정하면 계속
반복되고, **달력 시각과 무관하게** 동작하므로 시각 동기화가 필요 없습니다.

인터럽트 콜백에서는 플래그만 세우고 `main()` 루프에서 리셋합니다.
(ISR 안에서 바로 리셋해도 되지만, UART 로그를 끝까지 내보내기 위함)

### 18.2시간을 넘는 주기는 17비트 모드가 필요합니다

WUT 카운터는 16비트라서 `ck_spre`(1 Hz) 기준 **최대 65536초 ≈ 18.2시간**입니다.
그보다 긴 주기는 `WUCKSEL[2:1] = 11` (**CK_SPRE_17BITS**) 모드로 카운터에
**2¹⁶(65536)을 더해야** 합니다.

| 모드 | 주기 계산 | 범위 |
|---|---|---|
| `CK_SPRE_16BITS` | `(WUT + 1)` 초 | 1 s ~ 65536 s (~18.2 h) |
| `CK_SPRE_17BITS` | `(WUT + 1 + 65536)` 초 | 65537 s ~ 131072 s (~36.4 h) |

`main.h` 가 `RESET_PERIOD_SEC` 값을 보고 **모드와 카운터를 컴파일 타임에 자동
계산**하므로, 주기를 바꿀 때 이 계산을 신경 쓸 필요는 없습니다.

---

## 1-1. 저전력 모드에 진입하지 않습니다

"Wakeup Timer" 라는 이름은 저전력 모드에서 MCU 를 **깨울 수 있다**는 뜻이지
저전력 모드에 **들어가야 한다**는 뜻이 아닙니다. WUT 는 RTC 안에서 독립적으로
도는 주기 타이머일 뿐이고, MCU 가 풀스피드로 돌든 STOP 모드에 있든 상관없이
카운트해서 인터럽트를 겁니다.

이 프로젝트에는 저전력 진입 코드가 **하나도 없습니다.**

| 사용하지 않는 API | 상태 |
|---|---|
| `HAL_PWR_EnterSLEEPMode()` | ❌ 없음 |
| `HAL_PWR_EnterSTOPMode()` / `HAL_PWREx_EnterSTOP0/1/2Mode()` | ❌ 없음 |
| `HAL_PWR_EnterSTANDBYMode()` / `HAL_PWREx_EnterSHUTDOWNMode()` | ❌ 없음 |
| `__WFI()` / `__WFE()` | ❌ 없음 |

`main()` 의 `while(1)` 은 `HAL_GetTick()` 을 폴링하며 LED 를 토글하는
**완전 활성 상태의 busy loop** 입니다. MCU 는 리셋 시점까지 계속 동작하고,
애플리케이션 코드는 `/* USER CODE BEGIN 3 */` 에 넣으면 됩니다.

> 반대로 저전력이 필요해지면 이 루프 안에 `HAL_PWREx_EnterSTOP2Mode()` 한 줄만
> 넣으면 됩니다. RTC 는 STOP 모드에서도 계속 돌기 때문에 나머지는 그대로입니다.

---

## 1-2. 내부 LSI 를 쓸 때의 정확도

LSI 는 온칩 RC 발진기라 **외부 부품이 필요 없고 기동이 빠르며 실패하지
않는다**는 장점이 있지만, **주파수 오차가 ±5% 수준**(데이터시트 기준, 온도·전압
조건에 따라 변동)입니다. 이 오차는 그대로 리셋 주기의 오차가 됩니다.

| 설정 주기 | LSI ±5% 기준 실제 리셋 시점 |
|---|---|
| 1분 | 57초 ~ 63초 |
| 30분 | 28.5분 ~ 31.5분 |
| 1시간 | 57분 ~ 63분 |
| 24시간 | 22.8시간 ~ 25.2시간 |
| **30시간** | **28.5시간 ~ 31.5시간 (±1.5시간)** |

"대략 하루에 한 번 리프레시" 같은 용도라면 문제없지만, **분 단위 정확도가
필요하다면 LSI 로는 불가능**합니다. 그 경우 32.768 kHz 외부 크리스탈(LSE)이
필요하고, 회로가 바뀌므로 별도 검토가 필요합니다.

> 실제 오차는 개체·온도마다 다르므로, 정확도가 중요하면 보드에서 한 번
> 실측해 보고 `RESET_PERIOD_SEC` 를 보정하는 방법도 있습니다.

---

## 1-3. 백업 전원(VBAT) 구성 — 이 프로젝트의 전제

**VBAT 에 별도 배터리/슈퍼캡이 없고, VBAT 가 MCU 전원(VDD)에 연결되어 있다고
가정합니다.** 즉 백업 도메인(RTC, TAMP 백업 레지스터)은 MCU 전원이 살아있는
동안에만 유지됩니다.

| 이벤트 | RTC 달력 | 백업 레지스터 | WUT 설정 |
|---|---|---|---|
| **소프트웨어 리셋** (`HAL_NVIC_SystemReset()`) | ✅ 유지 | ✅ 유지 | ✅ 유지 |
| **NRST 핀 리셋 / 디버거 리셋** | ✅ 유지 | ✅ 유지 | ✅ 유지 |
| **전원 off → on** | ❌ 초기화 | ❌ 초기화 | ❌ 초기화 |

**소프트웨어 리셋으로는 백업 도메인이 지워지지 않고, 이건 VBAT 배선과
무관합니다.** 리셋 직후에도 RTC 는 멈추지 않고 계속 돌기 때문에 주기 리셋
동작은 배터리 유무와 상관없이 그대로입니다.

또한 이 프로젝트는 **부팅 시점 기준**으로 카운트하므로, 전원을 껐다 켜면
그 시점부터 다시 세기 시작하는 것이 정상 동작입니다. 달력 시각이 지워지는
것도 문제가 되지 않습니다.

콜드/웜 부트는 백업 레지스터의 매직 값으로 판별해 로그에 표시합니다.

```
 Boot type   : COLD  (power-on, backup domain cleared)   <- 전원 인가
 Boot type   : WARM  (reset only, backup domain kept)    <- 소프트/NRST 리셋
 Soft resets since power-on : 3
```

> 리셋 횟수는 **"이번 전원 인가 이후"** 의 누적값이며, 전원을 내리면 0 이 됩니다.
> RTC 달력도 콜드 부트마다 `2000-01-01 00:00:00` 으로 초기화되는데, 오히려
> 이게 주기 검증에 편합니다 (30시간 뒤 로그가 `2000-01-02 06:00:00` 이면 정확).

---

## 1-4. 살아있음(heartbeat) 로그

계속 동작 중인지 확인하기 쉽도록 **1분마다 uptime 과 리셋까지 남은 시간**을
UART 로 출력합니다.

```c
#define USE_HEARTBEAT_LOG     1U    /* 0 이면 출력 안 함 */
#define HEARTBEAT_PERIOD_SEC  60U   /* 출력 주기 [초] */
```

```
[ALIVE] uptime 00:01:00 | RTC 2000-01-01 00:01:00 | reset in 107940 s (29h 59m)
[ALIVE] uptime 00:02:00 | RTC 2000-01-01 00:02:00 | reset in 107880 s (29h 58m)
```

LED 하트비트(500 ms 토글)와 함께, MCU 가 잠들지 않고 도는지 육안/로그 양쪽으로
확인할 수 있습니다.

---

## 2. 파일 구성

```
.
├── STM32L562_RTC_WakeUp_Reset.ioc   # CubeMX 설정 파일 (시작점)
├── Core/
│   ├── Inc/
│   │   ├── main.h                   # 주기, 핀, 로그 설정
│   │   └── stm32l5xx_it.h
│   └── Src/
│       ├── main.c                   # RTC 초기화 + 주기 리셋 로직
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
- **RCC** → `Low Speed Clock (LSE)`: **Disable 그대로** (내부 LSI 사용)
- **Clock Configuration 탭**
  - System Clock Mux: **MSI (4 MHz)** — 기본값 그대로
  - **RTC Clock Mux: `LSI`**

### 3-3. RTC 설정 (Timers → RTC)
- **Mode**
  - ☑ `Activate Clock Source`
  - ☑ `Activate Calendar`
  - `WakeUp` → **`Internal WakeUp`**
- **Configuration → Parameter Settings**

  | 항목 | 값 |
  |------|------|
  | Asynchronous Predivider value | `127` |
  | Synchronous Predivider value | `249` |
  | Wake Up Clock | `RTC_WAKEUPCLOCK_CK_SPRE_17BITS` |
  | Wake Up Counter | `42463` (30시간) |

  → `(127+1) × (249+1) = 32000` 이 되어 LSI 32 kHz 에서 `ck_spre = 1 Hz`
  → `42463 + 1 + 65536 = 108000초 = 30시간`
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
2. 생성된 프로젝트의 아래 파일을 이 저장소의 파일로 덮어쓰기
   - `Core/Inc/main.h`, `Core/Src/main.c`,
     `Core/Src/stm32l5xx_hal_msp.c`, `Core/Src/stm32l5xx_it.c`
3. 빌드(Ctrl+B) 후 다운로드/디버그 실행

> 핵심 코드는 모두 `/* USER CODE BEGIN ... END */` 블록 안에 있어서
> CubeMX 에서 재생성해도 보존됩니다.

---

## 4. 리셋 주기 변경

`Core/Inc/main.h` 의 값 하나만 바꾸면 모드와 카운터는 자동 계산됩니다.

```c
#define RESET_PERIOD_SEC   (30U * 3600U)   /* 108000초 = 30시간 */
```

| 원하는 주기 | 설정 값 | 자동 선택되는 모드 / 카운터 |
|------|------|------|
| 1분 | `60U` | 16BITS / 59 |
| 5분 | `(5U * 60U)` | 16BITS / 299 |
| 30분 | `(30U * 60U)` | 16BITS / 1799 |
| 1시간 | `(1U * 3600U)` | 16BITS / 3599 |
| 12시간 | `(12U * 3600U)` | 16BITS / 43199 |
| 18.2시간 | `65536U` | 16BITS / 65535 (16비트 경계) |
| 24시간 | `(24U * 3600U)` | 17BITS / 20863 |
| **30시간** | `(30U * 3600U)` | **17BITS / 42463** |
| 1분 미만 / 30시간 초과 | — | ❌ `#error` 로 빌드 실패 |

> 30시간은 검증에 하루 넘게 걸리므로, 동작 확인은 `60U`(1분) 로 줄여서 하고
> 확인 후 되돌리는 것을 권장합니다.

---

## 5. 핵심 코드

### 5-1. 주기 → 모드/카운터 자동 계산 (`main.h`)
```c
#define RESET_PERIOD_SEC      (30U * 3600U)   /* 108000초 = 30시간 */

/* 지원 범위 검사 */
#if   (RESET_PERIOD_SEC < 60U)
  #error "RESET_PERIOD_SEC 는 60초(1분) 이상이어야 합니다."
#elif (RESET_PERIOD_SEC > 108000U)
  #error "RESET_PERIOD_SEC 는 108000초(30시간) 이하여야 합니다."
#endif

#if   (RESET_PERIOD_SEC <= 65536U)          /* 최대 약 18.2시간 */
  #define WUT_CLOCK_SEL   RTC_WAKEUPCLOCK_CK_SPRE_16BITS
  #define WUT_COUNTER     (RESET_PERIOD_SEC - 1U)
#elif (RESET_PERIOD_SEC <= 131072U)         /* 최대 약 36.4시간 */
  #define WUT_CLOCK_SEL   RTC_WAKEUPCLOCK_CK_SPRE_17BITS
  #define WUT_COUNTER     (RESET_PERIOD_SEC - 65536U - 1U)
#endif
```

### 5-2. RTC / Wakeup Timer 설정 (`MX_RTC_Init`)
```c
/* LSI 32000 Hz : (127+1) * (249+1) = 32000 -> ck_spre = 1 Hz */
hrtc.Init.AsynchPrediv = 127;
hrtc.Init.SynchPrediv  = 249;
...
/* 30시간 -> CK_SPRE_17BITS, WUT = 108000 - 65536 - 1 = 42463 */
if (HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, WUT_COUNTER,
                                WUT_CLOCK_SEL, 0U) != HAL_OK)
{
  Error_Handler();
}
```
> HAL 버전이 오래되어 인자가 3개뿐이라면 마지막 `0U`(WakeUpAutoClr)를 빼세요.

### 5-3. 인터럽트 핸들러 (`stm32l5xx_it.c`)
```c
void RTC_IRQHandler(void)
{
  HAL_RTCEx_WakeUpTimerIRQHandler(&hrtc);
}
```

### 5-4. 콜백 → 리셋 (`main.c`)
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

---

## 6. STM32L5 사용 시 주의사항

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

4. **`HAL_RTC_GetTime()` 뒤에는 반드시 `HAL_RTC_GetDate()` 를 호출하세요.**
   GetTime 이 shadow register 를 잠그고 GetDate 가 해제합니다.
   빠뜨리면 다음 읽기부터 시각이 갱신되지 않습니다.

5. **달력 재설정 금지.**
   소프트 리셋 후 `HAL_RTC_SetTime()` 을 다시 호출하면 시각이 초기화되어
   경과 시간을 확인할 수 없게 됩니다. `RTC_ICSR.INITS` 비트를 검사해
   **콜드 부트일 때만** 달력을 설정합니다.

6. **리셋 원인은 `RCC->CSR` 의 `SFTRSTF` 로 확인합니다.**
   읽은 뒤 `__HAL_RCC_CLEAR_RESET_FLAGS()` 로 지워야 다음 리셋 원인을
   구분할 수 있습니다.

7. **IWDG 로는 대체할 수 없습니다.**
   독립 워치독은 최대 타임아웃이 약 32초라 분 단위 이상의 주기 리셋에는
   사용할 수 없습니다.

---

## 7. 실행 결과 예시 (115200 8N1)

```
==========================================
 STM32L562 RTC WakeUp Timer Reset
==========================================
 Reset cause : NRST-PIN (CSR=0x0C000000)
 Boot type   : COLD  (power-on, backup domain cleared)
 RTC clock   : LSI ~32kHz (internal, +/-5%)
 Soft resets since power-on : 0
 RTC time    : 2000-01-01 00:00:00
 Trigger     : RTC WakeUp Timer
 Next reset in 108000 s (30h 00m)
------------------------------------------
[ALIVE] uptime 00:01:00 | RTC 2000-01-01 00:01:00 | reset in 107940 s (29h 59m)
[ALIVE] uptime 00:02:00 | RTC 2000-01-01 00:02:00 | reset in 107880 s (29h 58m)

... (1분마다 계속 출력, 30시간 경과) ...

[ALIVE] uptime 29:59:00 | RTC 2000-01-02 05:59:00 | reset in 60 s (0h 01m)
[RTC] 108000 s elapsed -> Software reset now!

==========================================
 STM32L562 RTC WakeUp Timer Reset
==========================================
 Reset cause : SOFTWARE (CSR=0x18000000)
 Boot type   : WARM  (reset only, backup domain kept)
 RTC clock   : LSI ~32kHz (internal, +/-5%)
 Soft resets since power-on : 1
 RTC time    : 2000-01-02 06:00:00
 Trigger     : RTC WakeUp Timer
 Next reset in 108000 s (30h 00m)
------------------------------------------
```

RTC 는 리셋 후에도 계속 동작하므로, 부팅할 때마다 찍히는 `RTC time` 이
설정한 주기만큼 증가하는지로 동작을 검증할 수 있습니다.
