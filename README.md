# STM32L562RET6 — RTC WakeUp Timer 주기적 Software Reset + RS485 / USB CDC 보고

> **다른 프로젝트**: ESP32-C3(ESP-AT) Wi-Fi AP / TCP 서버 접속 + 자동 재접속 (STM32L562CET6, 폴링 방식)
> → [`STM32L562_ESP32C3_WiFi/`](STM32L562_ESP32C3_WiFi/README.md)

STM32CubeMX / STM32CubeIDE 기반. **부팅 시점으로부터 일정 시간이 지나면
스스로 소프트웨어 리셋**하고, **리셋 사실과 누적 횟수를 USART3(RS485) 와
USB CDC(가상 COM 포트) 양쪽으로 PC 에 전송**합니다.

- 대상 : **STM32L562RET6 (LQFP64, Flash 512KB)**, TrustZone Disabled
- 리셋 주기 : **분 단위 / 시간 단위** 중 선택 (기본 **5분**)
- 보고 채널 (**같은 내용이 동시에 나갑니다**)
  - **USART3 RS485** — PB10 TX / PB11 RX / PB14 DE, 115200-8-N-1
  - **USB CDC** — PA11 D−/ PA12 D+, 가상 COM 포트 (보레이트 무관)
  - `USE_RS485` / `USE_USB_CDC` 로 각각 켜고 끕니다. 명령 수신도 양쪽에서 받습니다
- RTC 클럭 : 내부 **LSI(~32 kHz)** 기본, 크리스탈이 있으면 LSE 로 전환 가능
- **VBAT 가 MCU 전원과 함께 on/off 되는 보드**를 전제로 설계 ([1-3절](#1-3-백업-전원vbat-이-vdd-와-함께-꺼진다-이-프로젝트의-전제))
- 저전력 모드에 진입하지 않고 **계속 동작**하다가 시간이 되면 리셋

---

## 1. 동작 원리

| 항목 | 내용 |
|------|------|
| 타이머 | RTC Wakeup Timer (WUT) |
| RTC 클럭 | 내부 **LSI ~32 kHz** (`AsynchPrediv=127`, `SynchPrediv=249`) |
| 카운트 클럭 | `ck_spre` = 1 Hz |
| 인터럽트 | `RTC_IRQn` → `HAL_RTCEx_WakeUpTimerEventCallback()` |
| 리셋 방법 | `HAL_NVIC_SystemReset()` (Cortex-M33 `AIRCR.SYSRESETREQ`) |
| 보고 | `USART3` RS485 Driver Enable 모드 + `USB_OTG_FS` CDC |

```
전원 ON
  └─ COLD BOOT  : 백업 레지스터에 매직값 없음
                  → 카운터 0 으로 시작, Flash 의 "전원 인가 횟수" +1
                  → RS485 / USB CDC 로 배너 전송 → WakeUp Timer 장전
       │
       │  MCU 는 계속 풀스피드로 동작 (저전력 모드 미사용)
       │  LED 500ms 토글 + 30초마다 [ALIVE] 로그
       ▼
  시간 만료 → RTC_IRQHandler → 콜백에서 플래그만 set
       │
       ▼
  main 루프
       ├─ 백업 레지스터에 "내가 거는 리셋" 표식 기록
       ├─ Flash 의 "총 리셋 횟수" +1 기록
       ├─ 두 채널로 "*** SOFTWARE RESET ... ***" 전송
       ├─ RS485 송신 완료(TC) 대기 + USB 마지막 패킷 전송 후 정상 분리
       │                              ← 없으면 메시지 꼬리가 잘린다
       └─ HAL_NVIC_SystemReset()
       │
       ▼
  WARM BOOT : 매직값 있음 + 표식 있음 → 횟수 +1 → 배너 전송 → 다시 장전 ... 반복
```

인터럽트 콜백에서는 **플래그만 세우고** `main()` 루프에서 리셋합니다.
ISR 안에서 바로 리셋하면 로그가 중간에서 끊깁니다.

애플리케이션은 `comm.c` 의 `COMM_Printf()` 하나만 호출하고, 그 안에서
RS485 와 USB CDC 양쪽으로 같은 바이트열을 내보냅니다. 채널을 늘리거나 빼도
`app_reset.c` 는 건드릴 필요가 없습니다.

### 1-0. 18.2시간을 넘는 주기는 소프트웨어로 나눠 장전합니다

WUT 카운터는 16비트라서 `ck_spre`(1 Hz) 기준 **한 번에 최대 65535초 ≈ 18.2시간**
입니다. 이 프로젝트는 주기가 그보다 길면 **여러 조각으로 나눠 이어서 장전**합니다.

```c
/* app_reset.c */
static void AppReset_ArmChunk(void)
{
  uint32_t chunk = (s_remain_sec > WUT_MAX_CHUNK_SEC) ? WUT_MAX_CHUNK_SEC : s_remain_sec;
  HAL_RTCEx_DeactivateWakeUpTimer(&hrtc);
  HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, chunk - 1U, RTC_WAKEUPCLOCK_CK_SPRE_16BITS, 0U);
  s_remain_sec -= chunk;
}
```

24시간 = 65535 + 20865 (2조각), 48시간 = 65535 + 65535 + 41730 (3조각).
마지막 조각이 끝났을 때만 리셋합니다.

> 하드웨어에는 `CK_SPRE_17BITS` 모드(카운터에 2¹⁶을 더해 최대 약 36.4시간)도
> 있지만, 이 프로젝트는 **한 가지 모드(16BITS)만 쓰는 대신 범위 제한을 없애는**
> 쪽을 택했습니다. 재장전에 드는 시간은 18.2시간당 수 ms 수준이라 LSI 오차
> (±5%)에 비하면 무시할 수 있습니다.

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

`main()` 의 `while(1)` 은 `AppReset_Task()` 를 폴링하는 **완전 활성 상태의
busy loop** 입니다. 애플리케이션 코드는 `/* USER CODE BEGIN 3 */` 에 넣으면 됩니다.

> 반대로 저전력이 필요해지면 이 루프 안에 `HAL_PWREx_EnterSTOP2Mode()` 한 줄만
> 넣으면 됩니다. RTC 는 STOP 모드에서도 계속 돌기 때문에 나머지는 그대로입니다.
> (단, STOP 에서는 RS485 수신 명령이 동작하지 않습니다)

---

## 1-2. 내부 LSI 를 쓸 때의 정확도

LSI 는 온칩 RC 발진기라 **외부 부품이 필요 없고 기동이 빠르며 실패하지
않는다**는 장점이 있지만, **주파수 오차가 ±5% 수준**(데이터시트 기준, 온도·전압
조건에 따라 변동)입니다. 이 오차는 그대로 리셋 주기의 오차가 됩니다.

| 설정 주기 | LSI ±5% 기준 실제 리셋 시점 |
|---|---|
| 1분 | 57초 ~ 63초 |
| **5분** (기본) | **4분 45초 ~ 5분 15초** |
| 30분 | 28.5분 ~ 31.5분 |
| 1시간 | 57분 ~ 63분 |
| 24시간 | 22.8시간 ~ 25.2시간 (±1.2시간) |

"대략 N분/N시간마다 리프레시" 용도라면 문제없지만, **정확도가 중요하면
32.768 kHz 외부 크리스탈(LSE)** 이 필요합니다. 크리스탈이 실제로 달려 있다면
`main.h` 한 줄로 전환됩니다.

```c
#define RTC_CLOCK_LSE   1U    /* 크리스탈이 실제로 있을 때만 1 로! */
```

> **크리스탈이 없는데 `1` 로 두면 부팅이 멈춥니다.** `HAL_RCC_OscConfig()` 가
> LSE 기동 타임아웃 후 `Error_Handler()` 로 빠지고, 상태 LED 가 빠르게
> 깜빡입니다. 그럴 땐 `0` 으로 되돌리세요.
>
> 실측 보정도 가능합니다. 로그의 `RTC time` 이 설정 주기보다 일정 비율로
> 빠르거나 느리면 `RESET_PERIOD_VALUE` 를 그만큼 보정하면 됩니다.

---

## 1-3. 백업 전원(VBAT)이 VDD 와 함께 꺼진다 — 이 프로젝트의 전제

**VBAT 에 별도 배터리/슈퍼캡이 없고 VBAT 가 MCU 전원(VDD)에 연결되어 있습니다.**
즉 백업 도메인(RTC, TAMP 백업 레지스터)은 MCU 전원이 살아있는 동안에만 유지됩니다.

| 이벤트 | RTC 달력 | 백업 레지스터 | WUT 설정 | 내부 Flash |
|---|---|---|---|---|
| **소프트웨어 리셋** (`HAL_NVIC_SystemReset()`) | ✅ 유지 | ✅ 유지 | ✅ 유지 | ✅ 유지 |
| **NRST 핀 리셋 / 디버거 리셋** | ✅ 유지 | ✅ 유지 | ✅ 유지 | ✅ 유지 |
| **전원 off → on** | ❌ 초기화 | ❌ 초기화 | ❌ 초기화 | ✅ 유지 |

**소프트웨어 리셋으로는 백업 도메인이 지워지지 않고, 이건 VBAT 배선과
무관합니다.** 리셋 직후에도 RTC 는 멈추지 않고 계속 돌기 때문에 주기 리셋
동작 자체는 배터리 유무와 상관없이 그대로입니다.

문제가 되는 건 **"리셋 횟수"** 입니다. 전원을 내리면 백업 레지스터가 0 이 되므로
백업 레지스터만으로는 전원 사이클을 넘어선 횟수를 셀 수 없습니다.
그래서 횟수를 두 군데에 나눠서 관리합니다.

| 저장 위치 | 내용 | 소프트 리셋 | 전원 OFF→ON |
|---|---|---|---|
| TAMP 백업 레지스터 | 전원 인가 후 리셋/부팅 횟수, 누적 동작 시간, 실행 중 바뀐 주기 | **유지** | **소실** |
| 내부 Flash 마지막 페이지 | 총 리셋 횟수, 전원 인가 횟수 | 유지 | **유지** |

### ① 콜드 부트 판별 — 백업 레지스터 매직값

```c
if (BKP_Read(BKP_REG_MAGIC) != BKP_MAGIC_VALUE)  /* COLD : 전원을 새로 넣었다 */
```

`RCC->CSR` 의 리셋 플래그만으로는 구분이 애매합니다. NRST 핀 리셋과 전원
인가(BOR)가 둘 다 `PINRSTF` 를 세우는 경우가 있는데, 전자는 백업 도메인이
살아있고 후자는 지워집니다. 매직값 방식은 이 차이를 정확히 잡아냅니다.

### ② "내가 건 리셋" 표식 — 디버거 리셋과 구분

`SFTRSTF` 플래그만 보면 디버거가 건 리셋이나 다른 코드가 부른
`NVIC_SystemReset()` 까지 카운트에 섞입니다. 그래서 리셋 **직전에**
`BKP_REG_PENDING` 에 표식(`0x52535421`)을 남기고, 부팅 시 표식이 있을 때만
RTC 리셋으로 셉니다.

### ③ 전원을 넘어가는 누적 횟수는 내부 Flash 에

`flash_counter.c` 가 내부 Flash **마지막 페이지**(STM32L562RET6 = `0x0807F800`,
256KB 품목인 RC 라면 `0x0803F800`)에 16바이트 레코드를 덧붙여 기록합니다.

- 레코드 = `[64bit 데이터][데이터의 보수]` — 쓰다 만 레코드를 걸러냅니다.
- 페이지가 꽉 차면(128개) 지우고 처음부터 다시 씁니다.
  → **128번 저장마다 erase 1번.** Flash 지우기 수명 10,000회 기준
  1,280,000번 저장 가능. 5분 주기(하루 288회)면 약 12년입니다.
- 저장 주소는 `FLASHSIZE_BASE` 의 **실제 칩 용량에서 런타임 계산**하므로
  512KB(RE) / 256KB(RC) 품목 모두 코드 수정 없이 동작합니다.
  듀얼뱅크(출하 기본, 2KB 페이지)와 싱글뱅크(4KB 페이지)도 `FLASH_OPTR.DBANK`
  를 보고 자동으로 맞춥니다. 512KB 듀얼뱅크에서는 뱅크2의 127번 페이지입니다.

> **주의** — 마지막 페이지를 데이터로 쓰므로 프로그램이 그 영역까지 커지면
> 안 됩니다. 이 예제는 수십 KB라 여유가 많지만, 코드가 커질 것 같으면
> 링커 스크립트(`STM32L562RETX_FLASH.ld`)의 `FLASH` `LENGTH` 를
> `512K` → `510K` 로 줄여 두세요.
>
> Flash 저장이 필요 없으면 `USE_FLASH_COUNTER` 를 `0` 으로 두세요.
> 그러면 "전원 인가 후 횟수"만 보고합니다.

### ④ RTC 달력 재초기화

`MX_RTC_Init()` 에서 `ICSR.INITS` 비트(달력이 한 번이라도 설정됐는지)를 보고,
설정된 적 있으면 달력을 다시 쓰지 않습니다. 소프트 리셋 후에는 RTC 가 계속
돌고 있으므로 건드리지 않고, 전원을 껐다 켜면 `INITS = 0` 이라 자동으로
`2000-01-01 00:00:00` 으로 다시 초기화됩니다.

오히려 이게 주기 검증에 편합니다 — 부팅할 때마다 찍히는 `RTC time` 이
설정 주기만큼 늘어나면 정상입니다.

---

## 1-4. 살아있음(heartbeat) 로그

계속 동작 중인지 확인하기 쉽도록 **30초마다 uptime 과 리셋까지 남은 시간**을
RS485 로 출력합니다.

```c
#define USE_HEARTBEAT_LOG     1U    /* 0 이면 출력 안 함 */
#define HEARTBEAT_PERIOD_SEC  30U   /* 출력 주기 [초] */
```

```
[ALIVE] up 00:00:30 | next reset in 00:04:30 | resets 0 (total 41)
[ALIVE] up 00:01:00 | next reset in 00:04:00 | resets 0 (total 41)
```

LED 하트비트(500 ms 토글)와 함께, MCU 가 잠들지 않고 도는지 육안/로그 양쪽으로
확인할 수 있습니다.

---

## 2. 하드웨어 연결

### 2-1. 핀 배치 (STM32L562RET6, LQFP64)

| 핀 | 기능 | 비고 |
|---|---|---|
| **PB10** | USART3_TX (AF7) | 트랜시버 **DI** |
| **PB11** | USART3_RX (AF7) | 트랜시버 **RO** |
| **PB14** | USART3_DE (AF7) | 트랜시버 **DE** (+ `/RE`) |
| **PA11** | USB_OTG_FS_DM | USB D− (`USE_USB_CDC 1` 일 때) |
| **PA12** | USB_OTG_FS_DP | USB D+ (`USE_USB_CDC 1` 일 때) |
| PA5 | 상태 LED (GPIO_Output) | 500ms 토글. 안 쓰면 `USE_STATUS_LED 0` |
| PA13/PA14 | SWDIO / SWCLK | 디버거 |

> PB14 가 이미 다른 용도로 쓰이고 있으면 `main.h` 의 `RS485_DE_PIN` /
> `RS485_DE_GPIO_PORT` 를 원하는 핀으로 바꾸고 `RS485_USE_HW_DE` 를 `0` 으로
> 두세요. 그러면 그 핀을 **일반 GPIO 로 소프트웨어 토글**합니다.
> (USART3_DE 대체 핀은 PB1 / PD2 / PD12 이지만 LQFP64 에서는 PB1 만 나옵니다.
> CubeMX 핀아웃 화면에서 핀을 클릭하면 그 핀이 지원하는 신호가 뜹니다.)

### 2-2. RS485 트랜시버 배선 (MAX3485 / SP3485 / SN65HVD3082 등)

```
  STM32L562                     트랜시버                RS485 선로
  PB10 (TX) ───────────────────► DI
  PB11 (RX) ◄─────────────────── RO
  PB14 (DE) ───────────┬───────► DE
                       └───────► /RE      ← 묶기를 권장 (아래 설명)
                                  A  ──────────► A  (+)
                                  B  ──────────► B  (−)
                                  GND ────────── GND
```

- **`/RE` 를 `DE` 와 묶는 것을 권장**합니다. 송신 중에는 수신이 꺼지므로
  자기 송신이 되돌아오는 에코가 없습니다.
- `/RE` 를 GND 에 고정해 항상 수신 상태로 두어도 동작합니다. 이 경우 자기
  송신이 그대로 되돌아오는데, `RS485_Write()` 가 송신 직후 수신 버퍼를 비워
  에코를 명령으로 오인하지 않도록 처리합니다.
- 선로 **양 끝단**에 120Ω 종단 저항. 필요하면 A/B 에 바이어스 저항
  (560Ω~1kΩ 풀업/풀다운)을 달아 유휴 상태를 확정하세요. 바이어스가 없으면
  아무도 송신하지 않을 때 선로가 플로팅이라 수신 에러가 자주 납니다.
  (`HAL_UART_ErrorCallback()` 에서 수신을 재무장하므로 멈추지는 않습니다)
- PC 쪽은 USB-RS485 컨버터를 쓰고 터미널을 **115200-8-N-1** 로 엽니다.

### 2-3. DE 타이밍

`RS485_USE_HW_DE = 1` 이면 USART3 하드웨어가 첫 바이트 전에 DE 를 올리고
마지막 바이트 뒤에 내려줍니다. 앞뒤 여유 시간은 샘플 단위로 지정합니다.

```c
#define RS485_DE_ASSERT_TIME    8U   /* 8/16 = 0.5 비트시간 */
#define RS485_DE_DEASSERT_TIME  8U
```

`0` 이면 소프트웨어 토글 모드이고, `HAL_UART_Transmit()` 후 `TC` 플래그를
기다린 뒤 DE 를 내립니다.

### 2-4. USB CDC — 배선과 "주기 리셋"과의 궁합

#### 배선

```
  STM32L562                         USB 커넥터 (Type-C / micro-B)
  PA11 ─────────────────────────────► D−
  PA12 ─────────────────────────────► D+
  GND  ─────────────────────────────► GND
  (VBUS 는 감지용으로만 쓰거나 미연결)
```

- **32 MHz 크리스탈이 필요 없습니다.** 내부 **HSI48 + CRS** 를 씁니다.
  CRS 가 호스트의 SOF(1 ms)를 기준으로 HSI48 을 계속 보정해 USB 규격
  (±0.25%)을 만족시킵니다. 설정은 `MX_USB_Clock_Init()` 에 있습니다.
- **`HAL_PWREx_EnableVddUSB()` 가 필수**입니다. 이걸 빠뜨리면 USB 트랜시버가
  아예 동작하지 않아 PC 가 장치를 인식조차 못 합니다. L4/L5 에서 "USB 가
  무반응"인 경우 대부분 이것입니다.
- D+/D− 는 90Ω 차동 임피던스로 짧고 나란히 배선하세요.

#### USB 를 켜면 시스템 클럭이 48 MHz 로 올라갑니다

USB FS 는 인터럽트를 제때 처리해야 열거(enumeration)가 되는데 MSI 4 MHz 로는
빠듯합니다. `USE_USB_CDC = 1` 이면 `main.c` 가 자동으로 MSI 를 48 MHz
(`RCC_MSIRANGE_11`)로 올립니다. USART3 는 그대로 HSI16 을 쓰므로 보레이트는
영향받지 않습니다.

#### ⚠ 리셋마다 COM 포트가 끊깁니다

이게 USB CDC 의 본질적인 제약입니다. 소프트웨어 리셋이 걸리면 USB 장치가
사라졌다가 다시 나타나므로, **PC 의 COM 포트도 매번 사라졌다 다시 생깁니다.**
PuTTY / TeraTerm 같은 터미널은 그 시점에 포트를 닫아버립니다.

코드에서 할 수 있는 만큼은 해 두었습니다.

| 문제 | 대응 |
|---|---|
| 리셋 직후 배너가 열거 전에 나가서 사라짐 | `COMM_Init()` 이 `USBD_STATE_CONFIGURED` 가 될 때까지 대기 (`USB_CDC_READY_TIMEOUT_MS`, 기본 2초) |
| 열거 직후 호스트가 아직 포트를 못 염 | 추가 대기 `USB_CDC_READY_EXTRA_MS` (기본 300 ms) |
| 리셋 시 호스트가 장치 제거를 늦게 인식 | 리셋 직전 `USBD_Stop()` 으로 정상 분리 |
| 케이블 미연결 시 송신에서 멈춤 | 열거 안 됐으면 즉시 버림, 호스트가 안 읽어가면 `USB_CDC_TX_TIMEOUT_MS` 후 포기 |

그래도 **터미널 쪽이 포트를 다시 열어줘야** 합니다. 끊김 없는 로그가 필요하면
**RS485 를 기준 채널로 쓰고 USB 는 현장 점검용으로 쓰는 것**을 권합니다.

자동 재접속이 필요하면 PC 에서 이 정도 스크립트면 충분합니다.

```python
# pip install pyserial
import serial, serial.tools.list_ports, time

VID_PID = (0x0483, 0x5740)          # ST 기본 CDC VID/PID

def find_port():
    for p in serial.tools.list_ports.comports():
        if (p.vid, p.pid) == VID_PID:
            return p.device
    return None

while True:
    port = find_port()
    if port is None:
        time.sleep(0.2); continue
    try:
        with serial.Serial(port, 115200, timeout=0.2) as ser:
            print(f"--- connected: {port} ---", flush=True)
            while True:
                data = ser.read(256)
                if data:
                    print(data.decode("utf-8", "replace"), end="", flush=True)
    except serial.SerialException:
        print("--- disconnected, waiting ---", flush=True)
        time.sleep(0.3)
```

> `USB_CDC_READY_TIMEOUT_MS` 는 **케이블을 안 꽂았을 때 매 부팅마다 그만큼
> 기다린다**는 뜻이기도 합니다. USB 를 가끔만 쓴다면 값을 줄이거나
> `USE_USB_CDC` 를 `0` 으로 두세요.

---

## 3. 파일 구성

```
.
├── STM32L562_RTC_WakeUp_Reset.ioc   # CubeMX 설정 파일 (시작점)
├── Core/
│   ├── Inc/
│   │   ├── main.h                   # ★ 모든 사용자 설정이 여기 있음
│   │   ├── app_reset.h
│   │   ├── comm.h                   # 보고 채널 분배
│   │   ├── rs485.h
│   │   ├── usb_cdc.h
│   │   ├── flash_counter.h
│   │   └── stm32l5xx_it.h
│   └── Src/
│       ├── main.c                   # 클럭/주변장치 초기화, main 루프
│       ├── app_reset.c              # 콜드·웜 판별, 카운터, WUT 장전, 리셋, 배너
│       ├── comm.c                   # COMM_Printf() → RS485 + USB CDC 동시 출력
│       ├── rs485.c                  # USART3 RS485(DE) 송수신
│       ├── usb_cdc.c                # USB CDC 송수신 래퍼 + OTG_FS_IRQHandler
│       ├── flash_counter.c          # Flash 기반 비휘발성 카운터
│       ├── stm32l5xx_hal_msp.c      # RTC/USART3 클럭·GPIO·NVIC
│       └── stm32l5xx_it.c           # RTC_IRQHandler, USART3_IRQHandler
└── README.md
```

> HAL 드라이버(`Drivers/`), 링커 스크립트, `startup_stm32l562xx.s`, `syscalls.c`,
> 그리고 **USB 미들웨어(`USB_DEVICE/`, `Middlewares/ST/STM32_USB_Device_Library/`)**
> 는 용량이 커서 포함하지 않았습니다. 4장대로 CubeMX/CubeIDE 에서 프로젝트를 만든 뒤
> 위 소스를 덮어쓰고 추가하면 바로 빌드됩니다.

### 채널 구조

```
  app_reset.c  ──► COMM_Printf() ─┬─► rs485.c   ──► USART3 (DE = PB14)
                                  └─► usb_cdc.c ──► CDC_Transmit_FS()
                                                    └─ ST USB Device Library
```

---

## 4. STM32CubeMX / CubeIDE 설정 순서

### 4-1. 프로젝트 생성

1. CubeMX → **File ▸ New Project** → MCU 선택기에서 `STM32L562RET6` 검색 →
   LQFP64 패키지 선택.
   - 256KB 품목(`STM32L562RCT6`)으로 바꿔도 그대로 동작합니다. 핀 배치와
     주변장치가 같고 Flash 용량만 다른데, Flash 저장 주소를 칩에서 읽어
     런타임에 계산하기 때문입니다.
2. **TrustZone 활성화 여부를 묻는 창에서 반드시 `Without TrustZone`
   (TZEN Disabled) 을 선택**합니다. 켜면 Secure/Non-secure 두 프로젝트가
   생성되고 RTC 인터럽트도 `RTC_S_IRQn` 으로 바뀝니다.

### 4-2. System Core

| 항목 | 설정 |
|---|---|
| **SYS** | Debug = `Serial Wire` |
| **RCC** | High Speed Clock (HSE) = `Disable` |
| | Low Speed Clock (LSE) = `Disable` (크리스탈이 있으면 `Crystal/Ceramic Resonator`) |
| **ICACHE** | Activated 체크 (1-way) |

### 4-3. RTC (Timers → RTC)

- **Activate Clock Source** 체크
- **Activate Calendar** 체크
- **WakeUp** → `Internal WakeUp` 체크
- Parameter Settings
  - Asynchronous Predivider value : `127`
  - Synchronous Predivider value : `249` (LSI) / `255` (LSE)
  - Wake Up Clock : `RTC_WAKEUPCLOCK_CK_SPRE_16BITS`
  - Wake Up Counter : `299` (아무 값이어도 됩니다. 실제 값은 코드에서 넣습니다)
- **NVIC Settings** → `RTC global interrupt` 체크, Preemption Priority `5`

### 4-4. USART3 (RS485)

- Mode : `Asynchronous`
- **Hardware Flow Control (RS485)** : `Driver Enable`
  → PB14 가 `USART3_DE` 로 잡힙니다.
  - CubeMX 버전에 따라 이 항목이 안 보일 수 있습니다. **없어도 됩니다.**
    `rs485.c` 가 `HAL_RS485Ex_Init()` 를 직접 호출하고,
    `stm32l5xx_hal_msp.c` 가 PB14 를 AF7 로 직접 설정합니다.
    다만 CubeMX 로 재생성할 때 핀이 겹치지 않도록 PB14 는 비워 두세요.
- Parameter Settings : 115200 / 8bit / None / 1stop
- **NVIC Settings** → `USART3 global interrupt` 체크, Preemption Priority `6`
  (PC 명령 수신용. `USE_COMM_CMD 0` 이면 불필요)
- 핀아웃에서 **PB10 = USART3_TX, PB11 = USART3_RX** 확인

### 4-5. USB (USB_OTG_FS + USB_DEVICE CDC)

`USE_USB_CDC = 0` 으로 쓸 거면 이 절은 건너뛰어도 됩니다.

1. **Connectivity ▸ USB_OTG_FS**
   - Mode : **`Device_Only`**
   - 핀아웃에서 **PA11 = USB_OTG_FS_DM, PA12 = USB_OTG_FS_DP** 확인
   - **NVIC Settings** → `USB OTG FS global interrupt` 체크, Preemption Priority `4`
     (RTC `5` / USART3 `6` 보다 높게 — USB 는 응답이 늦으면 열거에 실패합니다)
2. **Middleware and Software Packs ▸ USB_DEVICE**
   - Class For FS IP : **`Communication Device Class (Virtual Port Com)`**
   - Parameter Settings 의 제품명/VID/PID 는 기본값 그대로 두면 됩니다
     (ST 기본 VID `0x0483` / PID `0x5740`)
3. **RCC** → `HSI48` 을 활성화할 수 있으면 켜 둡니다.
   (CubeMX 에서 안 보여도 됩니다 — `MX_USB_Clock_Init()` 이 코드에서 켭니다)

### 4-6. GPIO

- **PA5** 클릭 → `GPIO_Output` → User Label `STATUS_LED`

### 4-7. Clock Configuration

| 항목 | 값 | 이유 |
|---|---|---|
| System Clock Mux | **MSI** — USB 사용 시 **48 MHz**, 미사용 시 4 MHz | USB FS 인터럽트를 제때 처리하려면 4 MHz 로는 빠듯 |
| **USART3 Clock Mux** | **HSI16** | 시스템 클럭과 분리해 보레이트 정확도 확보 |
| **USB Clock Mux** | **HSI48** | CRS 로 SOF 동기 → 크리스탈 불필요 |
| RTC Clock Mux | **LSI** (또는 LSE) | |

> USART3 를 HSI16 으로 쓰는 이유: 16 MHz ÷ 115200 = 138.9 → 오차 **−0.08%**.
> MSI 를 그대로 쓰면 −0.79% 라 장거리 RS485 에서 마진이 줄어듭니다.
>
> 클럭/Flash wait state 는 `main.c` 의 `SystemClock_Config()` 가 `USE_USB_CDC`
> 값에 따라 알아서 고릅니다. CubeMX 화면과 숫자가 달라도 코드가 기준입니다.

### 4-8. 코드 생성 & 소스 반영

- Project Manager → Toolchain `STM32CubeIDE` → **GENERATE CODE**
- 생성된 프로젝트에 이 저장소의 `Core/Inc/*.h`, `Core/Src/*.c` 를 덮어쓰기/추가
- **USB 를 쓰면 생성된 `USB_DEVICE/App/usbd_cdc_if.c` 에 두 군데를 추가**합니다.
  (CubeMX 로 재생성해도 USER CODE 구간이라 보존됩니다)

  ```c
  /* USER CODE BEGIN INCLUDE */
  #include "usb_cdc.h"
  /* USER CODE END INCLUDE */
  ```

  ```c
  static int8_t CDC_Receive_FS(uint8_t* Buf, uint32_t *Len)
  {
    /* USER CODE BEGIN 6 */
    USB_CDC_RxHandler(Buf, *Len);          /* ← 이 한 줄 추가 */
    USBD_CDC_SetRxBuffer(&hUsbDeviceFS, &Buf[0]);
    USBD_CDC_ReceivePacket(&hUsbDeviceFS);
    return (USBD_OK);
    /* USER CODE END 6 */
  }
  ```

  이 한 줄이 없으면 **USB 로 보낸 명령만** 동작하지 않습니다(출력은 정상).

- 생성된 `Core/Src/stm32l5xx_it.c` 에 `OTG_FS_IRQHandler` 가 들어 있으면
  **지우세요.** 이 프로젝트는 같은 핸들러를 `usb_cdc.c` 에 두고 있어서 그대로
  두면 중복 정의로 링크 에러가 납니다. (반대로 CubeMX 가 만든 이름이
  `OTG_FS_IRQHandler` 가 아니면 `main.h` 의 `USB_CDC_IRQ_HANDLER` 를 그 이름으로
  고치면 됩니다.)

- 빌드 → 다운로드 → 터미널 열기
  - RS485 : USB-RS485 컨버터, **115200-8-N-1**
  - USB CDC : 새로 생긴 COM 포트 (보레이트는 아무 값이나 무관)

---

## 5. 리셋 주기 변경 — 분 단위 / 시간 단위

`Core/Inc/main.h` 상단 두 줄만 고칩니다.

```c
#define RESET_PERIOD_UNIT       RESET_UNIT_MINUTE   /* 분 단위 */
#define RESET_PERIOD_VALUE      5U                  /* 5분마다 리셋 */
```

```c
#define RESET_PERIOD_UNIT       RESET_UNIT_HOUR     /* 시간 단위 */
#define RESET_PERIOD_VALUE      24U                 /* 24시간마다 리셋 */
```

| 예시 | UNIT | VALUE | 실제 주기 |
|---|---|---|---|
| 1분 | `RESET_UNIT_MINUTE` | `1U` | 60 s |
| 5분 | `RESET_UNIT_MINUTE` | `5U` | 300 s |
| 30분 | `RESET_UNIT_MINUTE` | `30U` | 1,800 s |
| 1시간 | `RESET_UNIT_HOUR` | `1U` | 3,600 s |
| 12시간 | `RESET_UNIT_HOUR` | `12U` | 43,200 s |
| 24시간 | `RESET_UNIT_HOUR` | `24U` | 86,400 s (2조각) |
| 48시간 | `RESET_UNIT_HOUR` | `48U` | 172,800 s (3조각) |

`VALUE` 는 1~1000 이며, 벗어나면 `#error` 로 빌드가 막힙니다.

### 실행 중 변경 — RS485 **또는 USB CDC** 로 문자 하나 보내기

| 명령 | 동작 |
|---|---|
| `s` 또는 `?` | 현재 상태 배너 다시 출력 |
| `r` | 즉시 소프트웨어 리셋 |
| `m` | 단위를 **분**으로 변경 |
| `h` | 단위를 **시간**으로 변경 |
| `+` / `-` | 값 1 증가 / 감소 (1~1000) |
| `t` | **10초 주기 테스트 모드** (동작 확인용) |
| `c` | 모든 카운터 초기화 (백업 레지스터 + Flash) |

명령은 **두 채널 중 아무 쪽으로 보내도** 됩니다. 응답(배너/로그)은 항상
양쪽으로 나갑니다.

실행 중 바꾼 값은 백업 레지스터에 저장되어 **소프트 리셋을 넘어 유지**되지만,
**전원을 껐다 켜면 `main.h` 의 컴파일 타임 기본값으로 돌아갑니다**
(VBAT 가 같이 꺼지기 때문). 필요 없으면 `USE_COMM_CMD` 를 `0` 으로 두세요.

> USB 로 보낸 명령이 안 먹으면 4-8 의 `USB_CDC_RxHandler()` 한 줄을 빠뜨린
> 것입니다. 출력은 정상인데 입력만 안 되는 게 이 경우의 증상입니다.

---

## 6. 실행 결과 예시

아래 내용이 **RS485 와 USB CDC 양쪽에 똑같이** 나옵니다.
전원을 넣고 5분 주기로 한 번 리셋된 상황입니다.

```
========================================================
 STM32L562RET6  RTC WakeUp -> Software Reset (RS485/USB)
========================================================
 Reset cause  : BOR/POR NRST-PIN (CSR=0x0C000000)
 Boot type    : COLD  (power ON - VBAT/backup domain cleared)
 RTC resets   : 0   (since power ON, backup reg)
 Boot count   : 1   (since power ON, backup reg)
 TOTAL resets : 41   (survives power OFF, flash @0x0807F800)
 Power cycles : 7   (survives power OFF)
 RTC clock    : LSI 32000Hz(+-5%)
 Reset period : 5 min  = 300 s (00:05:00)
 Run time     : 00:00:00  (accumulated since power ON)
 RTC time     : 2000-01-01 00:00:00
 Next reset in: 300 s
--------------------------------------------------------
 CMD: s=status  r=reset now  m=minute  h=hour  +/-=value
      t=test(10s)  c=clear counters
--------------------------------------------------------
[ALIVE] up 00:00:30 | next reset in 00:04:30 | resets 0 (total 41)
[ALIVE] up 00:01:00 | next reset in 00:04:00 | resets 0 (total 41)
      :
*** SOFTWARE RESET (RTC wakeup) : RTC reset #1, total #42 ***

========================================================
 STM32L562RET6  RTC WakeUp -> Software Reset (RS485/USB)
========================================================
 Reset cause  : SOFTWARE (CSR=0x10000000)
 Boot type    : WARM  *** RESET BY RTC WAKEUP TIMER ***
 RTC resets   : 1   (since power ON, backup reg)
 Boot count   : 2   (since power ON, backup reg)
 TOTAL resets : 42   (survives power OFF, flash @0x0807F800)
 Power cycles : 7   (survives power OFF)
 RTC clock    : LSI 32000Hz(+-5%)
 Reset period : 5 min  = 300 s (00:05:00)
 Run time     : 00:05:00  (accumulated since power ON)
 RTC time     : 2000-01-01 00:05:00
 Next reset in: 300 s
--------------------------------------------------------
```

- `RTC resets` : 이번에 전원을 넣은 뒤의 리셋 횟수 (전원을 끄면 0)
- `TOTAL resets` : 장비 설치 후 누적 리셋 횟수 (전원을 꺼도 유지)
- `Run time` : 이번에 전원을 넣은 뒤의 누적 동작 시간
- `RTC time` : 부팅할 때마다 주기만큼 늘어나면 정상 (전원 off 시 초기화)

---

## 7. 동작 확인 순서

1. **5분을 기다리지 말고** 터미널에서 `t` 를 보내세요 → 10초 뒤 리셋됩니다.
   `*** SOFTWARE RESET ***` 과 새 배너가 연달아 뜨면 전체 경로가 정상입니다.
2. `r` 로 즉시 리셋 → `Boot type : WARM *** RESET BY RTC WAKEUP TIMER ***`
   와 `RTC resets` 증가 확인.
3. 보드 전원을 껐다 켜기 → `Boot type : COLD`, `RTC resets : 0` 으로 돌아가지만
   `TOTAL resets` 는 유지되고 `Power cycles` 가 1 증가하는지 확인.
   **VBAT 공용 보드에서 의도한 동작이 이것입니다.**
4. 두 채널의 출력이 같은지 비교. RS485 는 리셋 중에도 연결이 유지되고,
   USB 는 매 리셋마다 COM 포트가 끊겼다 다시 붙습니다(2-4 참고).
5. `c` 로 카운터를 모두 0 으로 되돌리고 실사용 주기로 두기.

### 안 될 때

| 증상 | 먼저 볼 것 |
|---|---|
| RS485 글자가 깨짐 | 보레이트(115200), 종단 저항 120Ω, GND 공통 |
| RS485 아무것도 안 나옴 | DE 핀이 송신 중 HIGH 인지 (오실로스코프), A/B 극성 |
| PC 가 USB 장치를 아예 인식 못 함 | `HAL_PWREx_EnableVddUSB()` 호출 여부, D+/D− 배선, PA11/PA12 를 GPIO 로 잡아두지 않았는지 |
| USB 장치는 뜨는데 "알 수 없는 장치" | HSI48/CRS 설정, USB 인터럽트 우선순위(RTC 보다 높게) |
| USB 로 출력은 되는데 명령이 안 먹음 | `usbd_cdc_if.c` 의 `USB_CDC_RxHandler()` 한 줄 (4-8) |
| 리셋 후 USB 배너가 안 보임 | 터미널이 포트를 다시 열었는지 (2-4 의 스크립트 사용) |
| 부팅이 매번 2초씩 느림 | USB 케이블 미연결 상태. `USB_CDC_READY_TIMEOUT_MS` 를 줄이거나 `USE_USB_CDC 0` |

---

## 8. STM32L5 사용 시 주의사항

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

6. **리셋 원인 플래그는 1회성입니다.**
   `RCC->CSR` 를 읽은 뒤 `__HAL_RCC_CLEAR_RESET_FLAGS()` 로 지워야 다음 리셋
   원인을 구분할 수 있습니다. `AppReset_CaptureCause()` 를 `HAL_Init()` 직후
   **한 번만** 호출하세요.

7. **Flash 를 지우거나 쓸 때는 ICACHE 를 끄세요.**
   L5 는 명령어 캐시(ICACHE)가 있어 Flash 를 고치면 캐시 내용이 낡은 값이
   됩니다. `flash_counter.c` 가 `HAL_ICACHE_Disable()` / `Invalidate()` /
   `Enable()` 로 감싸서 처리합니다.

8. **USB 를 쓰려면 `HAL_PWREx_EnableVddUSB()` 가 반드시 필요합니다.**
   L5 는 USB 전원 도메인(VDDUSB)이 기본적으로 꺼져 있습니다. 이걸 빠뜨리면
   PC 가 장치를 인식조차 하지 못합니다. `MX_USB_Clock_Init()` 과 CubeMX 가
   생성하는 `HAL_PCD_MspInit()` 양쪽에 들어 있습니다(중복 호출은 무해).

9. **`CDC_Transmit_FS()` 는 넘긴 버퍼를 복사하지 않습니다.**
   함수가 리턴한 뒤에도 전송이 끝날 때까지 그 메모리를 건드리면 안 됩니다.
   `usb_cdc.c` 는 64바이트 버퍼 두 개를 번갈아 써서 이 문제를 피합니다.
   직접 `CDC_Transmit_FS()` 를 호출하는 코드를 추가한다면 같은 점을 주의하세요.

10. **USB 인터럽트를 RTC 보다 높은 우선순위로 두세요.**
    USB 는 열거 중 호스트의 요청에 제때 응답하지 못하면 실패합니다.
    이 프로젝트는 USB `4` / RTC `5` / USART3 `6` 으로 잡았습니다.

11. **IWDG 로는 대체할 수 없습니다.**
   독립 워치독은 최대 타임아웃이 약 32초라 분 단위 이상의 주기 리셋에는
   사용할 수 없습니다.

12. 디버거를 붙인 채로 리셋하면 세션이 끊어질 수 있습니다. 장주기 시험은
    RS485 로그로 보는 편이 낫습니다.

13. Flash 카운터는 리셋 **직전에** 기록합니다. 기록과 리셋 사이(수 ms)에
    전원이 끊기면 그 1회는 누락될 수 있습니다.
