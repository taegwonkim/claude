# STM32CubeMX 설정 가이드 (STM32H743 듀얼 인터리브 ADC 8채널)

이 문서는 STM32CubeMX(또는 STM32CubeIDE의 `.ioc` 편집기)로 이 프로젝트와 같은 설정을 만드는 방법입니다.
CubeMX가 초기화 코드(`MX_ADC1_Init()` 등)를 생성하고, `adc_dual.c`는 **CubeMX 모드**
(`ADC_DUAL_USE_CUBEMX_INIT = 1`)로 설정 확인, 캘리브레이션, 시작/정지, 콜백 처리만 맡습니다.

- 대상: **NUCLEO-H743ZI2** (STM32H743ZITx, HSE 8 MHz bypass)
- 결과: SYSCLK 400 MHz, ADC 커널 클럭 72 MHz(f_ADC 36 MHz), 16-bit, 8채널, 채널당 500 kSps
- 메뉴 이름은 CubeMX 버전에 따라 조금씩 다를 수 있습니다. 각 항목 옆에 생성 코드에 들어가는 **HAL 값**을 같이 적었으니, 이름이 다르면 HAL 값으로 맞추세요.

> `.ioc` 파일은 일부러 넣지 않았습니다. `.ioc`의 키 이름은 CubeMX 버전과 내부 MCU 데이터베이스에 따라 달라서,
> 손으로 쓴 파일은 CubeMX에서 열 때 설정이 빠지거나 바뀔 수 있습니다. 아래 순서대로 GUI에서 설정하는 편이 안전합니다.

---

## 0. 프로젝트 만들기

1. **File → New Project → Board Selector**에서 `NUCLEO-H743ZI2`를 선택합니다.
   - MCU Selector에서 `STM32H743ZITx`를 골라도 됩니다. 그때는 아래 RCC와 USART3 핀을 직접 설정하세요.
2. "Initialize all peripherals with their default Mode?"에는 **No**를 선택합니다.
   (이더넷, USB 등 쓰지 않는 주변장치와 핀 충돌을 막기 위해서입니다.)

---

## 1. Pinout & Configuration

### 1-1. System Core → RCC

| 항목 | 값 |
|---|---|
| High Speed Clock (HSE) | **BYPASS Clock Source** (ST-LINK MCO 8 MHz) |
| Low Speed Clock (LSE) | Disable |
| Parameter Settings → Power Regulator Voltage Scale | **Power Regulator Voltage Scale 1** |
| Parameter Settings → Supply Source | **PWR_LDO_SUPPLY** |

### 1-2. System Core → SYS

| 항목 | 값 |
|---|---|
| Debug | Serial Wire |
| Timebase Source | SysTick |

### 1-3. System Core → CORTEX_M7

| 항목 | 값 |
|---|---|
| CPU ICache | **Enabled** |
| CPU DCache | **Enabled** |
| MPU | 기본값 그대로 (`adc_dual.c`가 캐시 무효화로 DMA 버퍼 일관성을 맞춥니다) |

### 1-4. Analog → ADC1 (마스터)

**Mode 영역**에서 다음 4개 채널을 **Single-ended**로 체크합니다.

| 논리 채널 | ADC1 입력 | 핀 |
|---|---|---|
| CH0 | IN15 | PA3 |
| CH2 | IN10 | PC0 |
| CH4 | IN5  | PB1 |
| CH6 | IN3  | PA6 |

**Configuration → Parameter Settings**

| 그룹 | 항목 | 값 | HAL 값 |
|---|---|---|---|
| ADC_Settings (Multi-ADC) | Mode | **Dual interleaved mode only** | `ADC_DUALMODE_INTERL` |
| | Dual mode data management / DMA access | **32-bit 형식**(resolution 32 down to 10 bits) | `ADC_DUALMODEDATAFORMAT_32_10_BITS` |
| | Delay between 2 sampling phases | **5 Cycles** | `ADC_TWOSAMPLINGDELAY_5CYCLES` |
| ADC_Settings | Clock Prescaler | **Asynchronous clock mode divided by 1** (Rev.V) | `ADC_CLOCK_ASYNC_DIV1` |
| | Resolution | **ADC 16-bit resolution** | `ADC_RESOLUTION_16B` |
| | Scan Conversion Mode | **Enabled** | `ADC_SCAN_ENABLE` |
| | Continuous Conversion Mode | **Disabled** | |
| | Discontinuous Conversion Mode | Disabled | |
| | End Of Conversion Selection | **End of sequence of conversion** | `ADC_EOC_SEQ_CONV` |
| | Overrun behaviour | **Overrun data overwritten** | `ADC_OVR_DATA_OVERWRITTEN` |
| | Conversion Data Management Mode | **DMA Circular Mode** | `ADC_CONVERSIONDATA_DMA_CIRCULAR` |
| | Low Power Auto Wait | Disabled | |
| | Left Bit Shift | No bit shift | `ADC_LEFTBITSHIFT_NONE` |
| ADC_Regular_ConversionMode | Enable Regular Conversions | Enable | |
| | Enable Regular Oversampling | Disable | |
| | Number Of Conversion | **4** | |
| | External Trigger Conversion Source | **Timer 6 Trigger Out event** | `ADC_EXTERNALTRIG_T6_TRGO` |
| | External Trigger Conversion Edge | **Trigger detection on the rising edge** | `ADC_EXTERNALTRIGCONVEDGE_RISING` |
| Rank 1 | Channel / Sampling Time / Offset | **Channel 15** / **2.5 Cycles** / No offset | `ADC_SAMPLETIME_2CYCLES_5` |
| Rank 2 | | **Channel 10** / 2.5 Cycles / No offset | |
| Rank 3 | | **Channel 5** / 2.5 Cycles / No offset | |
| Rank 4 | | **Channel 3** / 2.5 Cycles / No offset | |
| ADC_Injected_ConversionMode | Enable Injected Conversions | Disable | |

> **실리콘 리비전 주의**: Rev.V(REV_ID 0x2003, 현재 판매되는 칩 대부분)는 ADC 안에서 클럭을 /2 하므로 **DIV1**이면 f_ADC = 36 MHz입니다.
> Rev.Y(REV_ID 0x1003)라면 **divided by 2**로 바꾸세요. 칩 리비전은 프로그램이 시작할 때 시리얼로 출력됩니다.

**Configuration → DMA Settings → Add**

| 항목 | 값 |
|---|---|
| DMA Request | **ADC1** |
| Stream | **DMA1 Stream 0** |
| Direction | Peripheral To Memory |
| Priority | **Very High** |
| Mode | **Circular** |
| Increment Address | Peripheral: 해제 / **Memory: 체크** |
| Data Width | Peripheral: **Word** / Memory: **Word** |
| Use Fifo | 해제 |

**Configuration → NVIC Settings**

| 항목 | 값 |
|---|---|
| ADC1 and ADC2 global interrupts | **Enabled** (오버런 감지용) |
| DMA1 stream0 global interrupt | **Enabled** (DMA Settings를 추가하면 자동으로 켜짐) |

### 1-5. Analog → ADC2 (슬레이브)

ADC1을 듀얼 모드로 바꾸면 ADC2는 자동으로 슬레이브가 되고, 트리거와 멀티모드 항목은 회색으로 잠깁니다.

**Mode 영역**에서 다음 4개 채널을 **Single-ended**로 체크합니다.

| 논리 채널 | ADC2 입력 | 핀 |
|---|---|---|
| CH1 | IN18 | PA4 |
| CH3 | IN19 | PA5 |
| CH5 | IN2  | PF13 |
| CH7 | IN6  | PF14 |

**Configuration → Parameter Settings**

| 항목 | 값 |
|---|---|
| Clock Prescaler, Resolution | ADC1과 **같게** (DIV1, 16-bit) |
| Scan Conversion Mode | **Enabled** |
| Continuous Conversion Mode | **Disabled** |
| End Of Conversion Selection | End of sequence of conversion |
| Overrun behaviour | Overrun data overwritten |
| Conversion Data Management Mode | **Regular Conversion data stored in DR register only** (`ADC_CONVERSIONDATA_DR`) |
| Number Of Conversion | **4** |
| Rank 1 ~ 4 | **Channel 18 / 19 / 2 / 6**, 모두 **2.5 Cycles** |

- ADC2에는 **DMA를 추가하지 마세요.** 두 ADC의 결과는 공통 레지스터(CDR)를 통해 ADC1의 DMA로 한꺼번에 전송됩니다.
- **ADC1과 ADC2의 샘플링 시간은 반드시 같아야 합니다.** 다르면 인터리브 타이밍이 어긋납니다.

### 1-6. Timers → TIM6

| 항목 | 값 |
|---|---|
| Activated | **체크** |
| Prescaler | 0 |
| Counter Mode | Up |
| Counter Period | **399** (200 MHz / 500 kHz − 1) |
| auto-reload preload | Enable |
| Trigger Event Selection (TRGO) | **Update Event** |

- TIM6 인터럽트는 켜지 않습니다.
- `ADC_Dual_Init()`이 `adc_dual.h`의 `ADC_DUAL_SCAN_RATE_HZ`에 맞춰 프리스케일러와 주기를 다시 계산해 덮어씁니다.
  샘플링 속도는 CubeMX가 아니라 `adc_dual.h`에서 바꾸면 됩니다.

### 1-7. Connectivity → USART3

| 항목 | 값 |
|---|---|
| Mode | Asynchronous |
| 핀 | **PD8 (TX), PD9 (RX)** (Nucleo 보드 템플릿이면 이미 지정됨) |
| Baud Rate / Word Length / Parity / Stop Bits | **115200** / 8 Bits / None / 1 |

### 1-8. 핀 확인

Pinout 화면에서 다음 8개 핀이 **ADCx_INPy**(아날로그)로 표시되는지 확인합니다.

```
PA3  PC0  PB1  PA6   → ADC1
PA4  PA5  PF13 PF14  → ADC2
```

---

## 2. Clock Configuration

| 위치 | 값 |
|---|---|
| PLL Source Mux | **HSE** (8 MHz) |
| DIVM1 / DIVN1 / DIVP1 / DIVQ1 / DIVR1 | **/1, ×100, /2**, /4, /2 → PLL1P = **400 MHz** |
| System Clock Mux | **PLLCLK** |
| D1CPRE | /1 → SYSCLK 400 MHz |
| HPRE | **/2** → AXI/AHB 200 MHz |
| D1PPRE / D2PPRE1 / D2PPRE2 / D3PPRE | **/2** → APB 100 MHz (APB1 타이머 200 MHz) |
| DIVM2 / DIVN2 / DIVP2 | **/1, ×36, /4** → PLL2P = **72 MHz** |
| ADC Clock Mux | **PLL2P** |

- PLL1, PLL2 모두 입력 8 MHz, VCO wide range입니다(CubeMX가 자동으로 고릅니다).
- 화면에 빨간 표시가 없어야 합니다. USART3 클럭은 기본값(PCLK1)으로 둡니다.

---

## 3. Project Manager

| 탭 | 항목 | 값 |
|---|---|---|
| Project | Toolchain / IDE | STM32CubeIDE (또는 사용하는 툴체인) |
| Code Generator | Generate peripheral initialization as a pair of '.c/.h' files per peripheral | 선택 사항 (켜도 `extern` 핸들을 쓰므로 동작함) |
| Advanced Settings | Generated Function Calls 순서 | **`MX_DMA_Init`이 `MX_ADC1_Init`보다 위에** 있어야 함 |

> **DMA 초기화 순서는 꼭 확인하세요.** CubeMX 버전에 따라 `MX_ADC1_Init()`이 `MX_DMA_Init()`보다 먼저 생성되는 경우가 있습니다.
> 그러면 DMA가 동작하지 않습니다. Advanced Settings에서 `MX_DMA_Init`을 위로 올리거나 main.c에서 호출 순서를 확인하세요.

**GENERATE CODE**를 누릅니다.

---

## 4. 생성된 프로젝트에 코드 추가

### 4-1. 파일 복사

| 이 저장소 | CubeIDE 프로젝트 |
|---|---|
| `Core/Inc/adc_dual.h` | `Core/Inc/` |
| `Core/Src/adc_dual.c` | `Core/Src/` |

`main.c`, `stm32h7xx_it.c`, `stm32h7xx_hal_msp.c`, `stm32h7xx_hal_conf.h`, `syscalls.c`는 **CubeMX가 생성한 것을 씁니다.**
(이 저장소의 `main.c`, `stm32h7xx_it.c`, `syscalls_min.c`는 Makefile 빌드용이므로 복사하지 마세요.)

### 4-2. CubeMX 모드 켜기

**Project → Properties → C/C++ Build → Settings → MCU GCC Compiler → Preprocessor → Define symbols**에 추가합니다.

```
ADC_DUAL_USE_CUBEMX_INIT=1
```

(또는 `adc_dual.h`의 `#define ADC_DUAL_USE_CUBEMX_INIT 0`을 `1`로 바꿔도 됩니다.)

### 4-3. 링커 스크립트에 `.RAM_D2` 추가

`STM32H743ZITX_FLASH.ld`의 `/DISCARD/ :` 바로 앞에 `Linker/RAM_D2_section.ld`의 블록을 붙여 넣습니다.

```ld
  .RAM_D2 (NOLOAD) :
  {
    . = ALIGN(32);
    *(.RAM_D2)
    *(.RAM_D2*)
    . = ALIGN(32);
  } >RAM_D2
```

MEMORY 블록에 `RAM_D2 (xrw) : ORIGIN = 0x30000000, LENGTH = 288K`가 있는지도 확인하세요.

### 4-4. main.c의 USER CODE 영역

CubeMX가 다시 생성해도 지워지지 않도록 **반드시 USER CODE BEGIN/END 사이**에 넣습니다.

```c
/* USER CODE BEGIN Includes */
#include "adc_dual.h"
#include <stdio.h>
/* USER CODE END Includes */
```

```c
/* USER CODE BEGIN 0 */
/* printf → USART3 (CubeIDE syscalls.c의 _write가 __io_putchar를 호출) */
int __io_putchar(int ch)
{
  uint8_t c = (uint8_t)ch;
  if (c == '\n')
  {
    uint8_t cr = '\r';
    HAL_UART_Transmit(&huart3, &cr, 1, 10);
  }
  HAL_UART_Transmit(&huart3, &c, 1, 10);
  return ch;
}

static void ADC_Dual_Print(void)
{
  static uint32_t last_blocks = 0;
  ADC_Dual_Stats_t st;

  ADC_Dual_GetStats(&st);
  printf("\nscans/s=%lu  errors=%lu\n",
         (unsigned long)((st.block_count - last_blocks) * ADC_DUAL_SCANS_PER_HALF),
         (unsigned long)ADC_Dual_GetErrorCount());
  last_blocks = st.block_count;

  for (uint32_t ch = 0; ch < ADC_DUAL_NUM_CH; ch++)
  {
    printf("CH%lu(%s) mean=%5u (%4lu mV)  min=%5u  max=%5u\n",
           (unsigned long)ch, (ch & 1U) ? "ADC2" : "ADC1",
           st.mean[ch], (unsigned long)ADC_Dual_RawToMilliVolt(st.mean[ch]),
           st.min[ch], st.max[ch]);
  }
}
/* USER CODE END 0 */
```

```c
  /* USER CODE BEGIN 2 */
  setvbuf(stdout, NULL, _IONBF, 0);
  printf("\nSTM32H743 dual interleaved ADC (CubeMX), REV_ID=0x%04lX\n",
         (unsigned long)HAL_GetREVID());

  /* MX_DMA_Init / MX_ADC1_Init / MX_ADC2_Init / MX_TIM6_Init 다음에 호출 */
  if (ADC_Dual_Init() != HAL_OK)
  {
    printf("ADC_Dual_Init: CubeMX 설정이 CubeMX_Setup.md와 다릅니다\n");
    Error_Handler();
  }
  if (ADC_Dual_Start() != HAL_OK)
  {
    Error_Handler();
  }
  uint32_t last_print = HAL_GetTick();
  uint32_t last_err = 0;
  /* USER CODE END 2 */
```

```c
    /* USER CODE BEGIN 3 */
    /* 오버런이 나면 ADC/DMA를 다시 시작합니다. */
    if (ADC_Dual_GetErrorCount() != last_err)
    {
      last_err = ADC_Dual_GetErrorCount();
      (void)ADC_Dual_Stop();
      if (ADC_Dual_Start() != HAL_OK)
      {
        Error_Handler();
      }
    }

    if ((HAL_GetTick() - last_print) >= 1000U)
    {
      last_print = HAL_GetTick();
      ADC_Dual_Print();
    }
  }   /* ← CubeMX가 생성한 while (1)의 닫는 괄호(이미 있음) */
  /* USER CODE END 3 */
```

> `USER CODE BEGIN 3`은 CubeMX가 만든 `while (1)` 루프 **안쪽**에 있습니다. 위 코드의 마지막 `}`는
> CubeMX가 생성한 `while (1) {`를 닫는 괄호이므로 따로 추가하지 마세요(생성된 파일의 괄호 구조를 그대로 두면 됩니다).

---

## 5. `ADC_Dual_Init()`이 확인하는 항목

CubeMX 모드에서 `ADC_Dual_Init()`은 다음 중 하나라도 다르면 `HAL_ERROR`를 돌려줍니다.

| 확인 항목 | 기대값 |
|---|---|
| `hadc1.Instance`, `hadc2.Instance` | ADC1, ADC2 |
| `hadc1.DMA_Handle` | NULL이 아님 (ADC1 DMA 설정됨) |
| `hadc1.Init.Resolution` | 16-bit |
| 두 ADC의 `NbrOfConversion` | 4 |
| `ADC12_COMMON->CCR` DUAL | Dual interleaved |
| `ADC12_COMMON->CCR` DAMDF | 32-bit 형식 |
| `htim6.Instance` | TIM6 |

여기서 확인하지 않는 항목(채널 번호, 샘플링 시간, 트리거 소스, DMA 모드/데이터 폭)은 위 표대로 맞춰 주세요.

---

## 6. 동작 확인

시리얼 터미널(115200-8-N-1)로 ST-LINK 가상 COM 포트를 엽니다.

- `scans/s`가 약 **500000**이면 TIM6 트리거와 DMA가 정상입니다.
  0이면 DMA 초기화 순서(3절), TIM6 TRGO, ADC1 트리거 소스를 확인하세요.
- `errors`가 계속 늘면 스캔 레이트를 낮추거나(`ADC_DUAL_SCAN_RATE_HZ`) 다른 DMA 사용을 줄이세요.
- 각 핀에 GND / 3.3 V를 넣어 해당 CH만 0 / 약 65535로 바뀌는지 확인합니다.
