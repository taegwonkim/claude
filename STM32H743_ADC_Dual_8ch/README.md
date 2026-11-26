# STM32H743 16-bit 듀얼 인터리브 ADC 8채널 예제

STM32H743의 **ADC1(마스터) + ADC2(슬레이브)를 듀얼 인터리브 모드**로 묶어
**16-bit, 8채널을 채널당 500 kSps(전체 4 Msps)**로 샘플링하는 프로젝트입니다.

- **TIM6 TRGO 트리거**: 샘플링 주기가 타이머로 정확히 고정됩니다.
- **DMA circular + 하프/풀 콜백**: CPU 개입 없이 연속으로 수집합니다.
- **`.RAM_D2` 섹션(D2 SRAM, 0x30000000)**에 DMA 버퍼를 두고 D-Cache 무효화를 처리합니다.
- **USART3(ST-LINK 가상 COM, 115200 bps)**로 1초마다 채널별 평균/최소/최대를 출력합니다.

기준 보드: **NUCLEO-H743ZI2** (HSE 8 MHz bypass)

> 상태: ST 공식 HAL(stm32h7xx-hal-driver)과 arm-none-eabi-gcc 13.2로 **경고 없이 빌드되는 것만 확인했습니다.**
> 실제 보드에서는 아직 검증하지 않았습니다. 처음 실행할 때는 아래 "검증 체크리스트"를 확인하세요.

---

## 1. 동작 구조

```
TIM6 (500 kHz) ──TRGO──► ADC1(마스터) rank1 ─ rank2 ─ rank3 ─ rank4
                              │ 지연(5 cycles)
                              ▼
                         ADC2(슬레이브) rank1 ─ rank2 ─ rank3 ─ rank4

ADC12_COMMON->CDR (32-bit) = [ADC2 결과 : ADC1 결과]  ──DMA1_Stream0──► s_dma_buf (.RAM_D2)
```

- 트리거 1회 = 스캔 1회 = 32비트 워드 4개 = 8채널 샘플
- 한 스캔 안의 샘플링 순서: `CH0 → CH1 → CH2 → … → CH7` (ADC1과 ADC2가 번갈아 샘플링)
- 워드 형식(DAMDF = 0b10): `bit[15:0] = ADC1`, `bit[31:16] = ADC2`

### 타이밍 (f_ADC = 36 MHz, 16-bit)

| 항목 | 값 |
|---|---|
| 샘플링 시간 | 2.5 cycles |
| 변환 1회 | 2.5 + 8.5 = 11 cycles = 0.306 µs |
| 스캔 1회(ADC당 4랭크 + 슬레이브 지연) | 4 × 11 + 5 = 49 cycles ≈ 1.36 µs → 이론상 최대 약 730 kHz |
| 설정 스캔 레이트 | **500 kHz** (채널당 500 kSps, 전체 4 Msps) |
| DMA 하프 버퍼 | 256 스캔 = 1024 워드 → **512 µs마다 콜백** |

### ADC 클럭

- ADC 커널 클럭은 PLL2P = 72 MHz입니다(HSE 8 MHz / 1 × 36 / 4).
- **Rev.V**: ADC 안에서 /2가 되므로 프리스케일러를 DIV1로 두면 f_ADC = 36 MHz입니다.
- **Rev.Y**: 내부 /2가 없으므로 코드가 `HAL_GetREVID()`로 리비전을 확인해 DIV2로 자동 설정합니다.

---

## 2. 핀 배치 (NUCLEO-H743ZI2 기준)

| 논리 채널 | ADC | 랭크 | 핀 | 입력 | Nucleo 커넥터 |
|---|---|---|---|---|---|
| CH0 | ADC1 | 1 | PA3  | ADC12_INP15  | A0 |
| CH1 | ADC2 | 1 | PA4  | ADC12_INP18  | CN7 |
| CH2 | ADC1 | 2 | PC0  | ADC123_INP10 | A1 |
| CH3 | ADC2 | 2 | PA5  | ADC12_INP19  | D13 |
| CH4 | ADC1 | 3 | PB1  | ADC12_INP5   | A3 |
| CH5 | ADC2 | 3 | PF13 | ADC2_INP2    | D7 |
| CH6 | ADC1 | 4 | PA6  | ADC12_INP3   | D12 |
| CH7 | ADC2 | 4 | PF14 | ADC2_INP6    | D4 |

- 이더넷(RMII: PA1, PA2, PA7, PC1, PC4, PC5 등), LED(PB0, PE1, PB14), VCP(PD8, PD9)와 겹치지 않는 핀입니다.
- 핀을 바꾸려면 `Core/Src/adc_dual.c`의 `s_master_pins[]`(ADC1에 연결 가능한 핀)와
  `s_slave_pins[]`(ADC2에 연결 가능한 핀)를 고치세요.
- **입력 전압은 0 ~ VREF+(3.3 V)를 넘으면 안 됩니다.**

---

## 3. 폴더 구조

```
STM32H743_ADC_Dual_8ch/
├── Core/
│   ├── Inc/
│   │   ├── adc_dual.h            # 설정값(스캔 레이트, 버퍼 크기, 샘플링 시간 등)과 API
│   │   ├── main.h
│   │   └── stm32h7xx_hal_conf.h  # 필요한 HAL 모듈만 켬, HSE_VALUE = 8 MHz
│   └── Src/
│       ├── adc_dual.c            # ADC/DMA/TIM6 초기화, 콜백, IRQ 핸들러
│       ├── main.c                # 클럭 400 MHz, UART3 출력, 메인 루프
│       ├── stm32h7xx_it.c        # 최소 예외 핸들러(Makefile 빌드용)
│       └── syscalls_min.c        # printf용 최소 시스템 콜(Makefile 빌드용)
├── Linker/
│   ├── STM32H743ZITX_FLASH.ld    # .RAM_D2 섹션이 들어간 전체 링커 스크립트
│   └── RAM_D2_section.ld         # CubeIDE 링커 스크립트에 붙여 넣을 블록
├── tools/fetch_drivers.sh        # ST HAL / CMSIS 받기
└── Makefile
```

---

## 4. 빌드 방법

### 방법 A: Makefile (arm-none-eabi-gcc)

```bash
./tools/fetch_drivers.sh
```

```bash
make -j
```

`build/adc_dual_8ch.elf`, `.hex`, `.bin`이 생성됩니다. STM32CubeProgrammer로 기록하세요.

```bash
STM32_Programmer_CLI -c port=SWD -w build/adc_dual_8ch.hex -v -rst
```

### 방법 B: STM32CubeIDE

1. **File → New → STM32 Project**에서 `STM32H743ZITx`(또는 NUCLEO-H743ZI2)를 선택합니다.
   보드 템플릿으로 만들 때 "Initialize all peripherals with their default Mode?"에는 **No**를 선택하세요.
2. `.ioc`에서 **ADC1, ADC2, TIM6, DMA, USART3를 설정하지 마세요.** 이 코드가 직접 초기화합니다.
   (설정하면 `hadc1`, `HAL_ADC_MspInit`, IRQ 핸들러가 중복됩니다.)
3. 다음 파일을 프로젝트에 복사합니다.
   - `Core/Inc/adc_dual.h`, `Core/Src/adc_dual.c`
   - `Core/Src/main.c` (생성된 main.c를 교체)
   - `stm32h7xx_hal_conf.h`는 생성된 것을 쓰되, **ADC, TIM, UART 모듈이 켜져 있어야 합니다.**
     꺼져 있으면 이 저장소의 파일로 교체하고, `Drivers/STM32H7xx_HAL_Driver/Src`에
     `stm32h7xx_hal_adc.c`, `_adc_ex.c`, `_tim.c`, `_tim_ex.c`, `_uart.c`, `_uart_ex.c`를 추가하세요
     (STM32Cube 펌웨어 저장소 `~/STM32Cube/Repository/STM32Cube_FW_H7_Vx.x.x/`에 있습니다).
   - `stm32h7xx_it.c`, `syscalls.c`는 **CubeIDE가 만든 것을 그대로** 씁니다.
4. CubeIDE의 `STM32H743ZITX_FLASH.ld`에서 `/DISCARD/` 앞에 `Linker/RAM_D2_section.ld`의 블록을 붙여 넣습니다.
5. 빌드하고 실행합니다. 터미널(115200-8-N-1)로 ST-LINK 가상 COM 포트를 엽니다.

출력 예:

```
STM32H743 dual interleaved ADC, 16-bit, 8ch @ 500000 Hz/ch
SYSCLK=400000000 Hz, REV_ID=0x2003

blocks/s=1953 (scans/s=499968)  errors=0
CH0(ADC1) mean=32801 (1652 mV)  min=32750  max=32860
CH1(ADC2) mean=  112 (   6 mV)  min=   80  max=  150
...
```

---

## 5. 데이터 사용 방법

`ADC_Dual_BlockCallback()`은 weak 함수입니다. 사용자 코드에서 다시 정의하면
하프 버퍼가 찰 때마다(512 µs마다) DMA 인터럽트 안에서 호출됩니다.
호출되는 시점에 캐시 무효화는 이미 끝나 있습니다.

```c
void ADC_Dual_BlockCallback(const uint32_t *raw, uint32_t scans)
{
  for (uint32_t s = 0; s < scans; s++) {
    const uint32_t *w = &raw[s * ADC_DUAL_RANKS_PER_ADC];
    uint16_t ch0 = w[0] & 0xFFFF;   // PA3 (ADC1)
    uint16_t ch1 = w[0] >> 16;      // PA4 (ADC2)
    uint16_t ch2 = w[1] & 0xFFFF;   // PC0 (ADC1)
    /* ... ch7 = w[3] >> 16 */
  }
}
```

- 처리 시간은 512 µs보다 충분히 짧아야 합니다. 무거운 처리는 데이터를 복사한 뒤 메인 루프에서 하세요.
- 콜백 안에서 `printf` 같은 느린 함수는 쓰지 마세요.

---

## 6. 설정 바꾸기 (`Core/Inc/adc_dual.h`)

| 매크로 | 기본값 | 설명 |
|---|---|---|
| `ADC_DUAL_SCAN_RATE_HZ` | 500000 | 채널당 샘플링 레이트. 이론상 최대 약 730 kHz(샘플링 2.5 cycles 기준), 실제로는 여유를 두세요 |
| `ADC_DUAL_SCANS_PER_HALF` | 256 | 콜백 한 번에 처리할 스캔 수 |
| `ADC_DUAL_SAMPLING_TIME` | `ADC_SAMPLETIME_2CYCLES_5` | 신호원 임피던스가 높으면 8.5 / 16.5 cycles로 늘리세요(최대 스캔 레이트는 줄어듭니다) |
| `ADC_DUAL_TWO_SAMPLING_DELAY` | 5 cycles | 마스터와 슬레이브 샘플링 사이의 간격 |
| `ADC_DUAL_VREF_MV` | 3300 | mV 변환용 기준 전압 |

샘플링 시간을 늘렸을 때 최대 스캔 레이트는 대략 다음과 같습니다.

```
최대 스캔 레이트 ≈ f_ADC / (4 × (샘플링 사이클 + 8.5) + 지연 사이클)
```

---

## 7. 주의할 점

1. **DMA 버퍼 위치**: DMA1/DMA2는 DTCM(0x20000000)에 접근할 수 없습니다. 버퍼는 반드시 `.RAM_D2`(또는 AXI SRAM)에 두세요.
2. **캐시 일관성**: 버퍼 주소와 하프 크기가 32바이트(캐시 라인)의 배수여야 합니다. `_Static_assert`로 검사합니다.
3. **오버런**: 오류 콜백에서 횟수를 세고, 메인 루프가 ADC/DMA를 다시 시작합니다. `errors`가 계속 늘면
   스캔 레이트를 낮추거나 샘플링 시간을 조정하세요.
4. **인터리브 오차**: ADC1과 ADC2는 각각 캘리브레이션(offset + linearity)하지만, 두 ADC 사이에
   작은 오프셋/게인 차이가 남을 수 있습니다. 정밀 측정이라면 채널별로 소프트웨어 보정을 추가하세요.
5. **아날로그 프런트엔드**: 2.5 cycles(약 70 ns) 샘플링에서 16-bit 정밀도를 얻으려면
   신호원 임피던스가 매우 낮아야 합니다. 고속 op-amp 버퍼와 RC 필터를 쓰세요.
   데이터시트의 "fast channel / slow channel" 구분과 채널별 최대 샘플링 속도도 확인하세요.
6. **클럭**: HSE가 8 MHz가 아니면 `main.c`의 PLL1, `adc_dual.c`의 PLL2, `stm32h7xx_hal_conf.h`의 `HSE_VALUE`를 함께 바꾸세요.

---

## 8. 검증 체크리스트 (보드에서 처음 실행할 때)

- [ ] 시리얼 출력의 `scans/s`가 약 500000인지 (타이머와 트리거가 정상인지)
- [ ] `errors=0`이 유지되는지 (오버런이 없는지)
- [ ] 각 입력에 GND / 3.3 V / 1.65 V(분압)를 넣었을 때 해당 CH만 0 / 65535 / 약 32768로 바뀌는지 (핀 매핑 확인)
- [ ] 같은 신호를 CH0(ADC1)과 CH1(ADC2)에 넣었을 때 평균값 차이가 작은지 (두 ADC 간 오차)

> 참고: RM0433은 인터리브 모드를 주로 "한 채널을 두 ADC가 번갈아 변환"하는 용도로 설명합니다.
> 이 예제는 ADC마다 4랭크 시퀀스를 두고 트리거 1회에 시퀀스 전체를 변환하는 구성입니다.
> 보드에서 `scans/s`와 핀 매핑이 기대대로 나오지 않으면, 같은 핀 테이블로
> **듀얼 레귤러 동시 모드(`ADC_DUALMODE_REGSIMULT`)**로 바꿔 보세요. 데이터 형식(CDR 32비트 묶음)과
> 나머지 코드는 그대로 쓸 수 있고, 같은 랭크의 두 채널이 동시에 샘플링된다는 점만 달라집니다.

## 참고 문서

- RM0433 STM32H742/743/750/753 Reference manual – ADC, "Dual ADC modes – Interleaved mode"
- DS12110 STM32H743xI 데이터시트 – ADC 특성, 핀 정의
- AN5354 Getting started with the STM32H7 Series MCU 16-bit ADC
