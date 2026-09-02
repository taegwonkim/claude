# 01. STM32CubeMX 설정 (STM32L562CET6)

> 대상: STM32L562CET6 (LQFP48), STM32CubeIDE 내장 CubeMX 또는 standalone CubeMX
> 펌웨어 패키지: STM32Cube FW_L5 V1.5.x 이상

---

## 1. 프로젝트 생성

| 항목 | 값 |
|---|---|
| Part Number | STM32L562CETx |
| Package | LQFP48 |
| **TrustZone** | **Disabled** (Non-secure only) |
| Toolchain | STM32CubeIDE |
| Code generator | *Generate peripheral initialization as a pair of .c/.h files* 체크 |

> TrustZone을 켜면 Secure/NonSecure 두 개의 프로젝트가 생성되어 구조가 복잡해집니다.
> 본 프로젝트는 TZEN=0 (RDP Level 0, 단일 이미지) 기준입니다.
> 프로젝트 생성 시 나오는 "Do you want to enable TrustZone" 질문에 **No**.

---

## 2. 클럭 (RCC / Clock Configuration)

### 2.1 RCC
| 항목 | 값 |
|---|---|
| High Speed Clock (HSE) | **Crystal/Ceramic Resonator** (8 MHz 외부 크리스탈 기준) |
| Low Speed Clock (LSE) | Crystal/Ceramic Resonator (32.768 kHz, RTC 사용 시) / 없으면 Disable |
| MSI | 기본값 유지 (4 MHz, 부팅용) |

> 외부 크리스탈이 없는 보드라면 HSE = Disable, PLL 소스를 MSI 4 MHz로 두고
> PLLM=1, PLLN=55, PLLR=2 → 110 MHz 로 맞추면 됩니다. (아래 표의 HSE 항목만 대체)

### 2.2 Clock Configuration 탭
| 항목 | 값 |
|---|---|
| PLL Source | HSE (8 MHz) |
| PLLM | /1 |
| PLLN | x55 |
| PLLP | /7 (미사용) |
| PLLQ | /2 |
| PLLR | /4 → **SYSCLK = 110 MHz** |
| SYSCLK Source | PLLCLK |
| AHB Prescaler | /1 → HCLK 110 MHz |
| APB1 Prescaler | /1 → PCLK1 110 MHz |
| APB2 Prescaler | /1 → PCLK2 110 MHz |
| **Voltage Scaling** | **Range 0 (Boost mode)** — 110 MHz 동작 필수 |
| Flash Latency | 5 WS (CubeMX 자동 계산) |
| **USB clock (48 MHz)** | **HSI48 + CRS** 사용 (RCC → "HSI48" Enable, CRS Sync Source = USB SOF) |

> HSE 8 MHz 기준: 8/1*55 = 440 MHz VCO, /4 = 110 MHz SYSCLK.
> VCO 입력은 4~16 MHz, VCO 출력은 64~344 MHz 범위여야 하므로 CubeMX가 경고를 내면
> PLLN을 조정하십시오. (예: PLLM=/2, PLLN=x55, PLLR=/2 → VCO 220 MHz, SYSCLK 110 MHz)

### 2.3 PWR 관련 코드 (생성 후 확인)
`SystemClock_Config()` 안에 아래가 포함되어야 합니다.
```c
HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE0);
```
그리고 `MX_USB_Device_Init()` 이전에 **VDDUSB 전원 유효화**가 필요합니다.
(main.c USER CODE 2 에 추가 — Integration/main_usercode.c 참조)
```c
HAL_PWREx_EnableVddUSB();
```

---

## 3. 핀 배치 (LQFP48)

| 기능 | 핀 | 모드 | 비고 |
|---|---|---|---|
| **USART1_TX** | PA9 | AF7 | → ESP32-C WROOM RX |
| **USART1_RX** | PA10 | AF7 | ← ESP32-C WROOM TX |
| ESP_EN (RESET) | PA4 | GPIO_Output, PP, No pull, **초기 HIGH** | ESP32 EN 핀 |
| **USART2_TX** | PA2 | AF7 | → FPGA (Cyclone IV) RX |
| **USART2_RX** | PA3 | AF7 | ← FPGA TX |
| **FPGA_TRIG** | PA1 | **GPIO_EXTI1, Falling edge, Pull-up** | FPGA 트리거 입력 |
| **USART3_TX** | PB10 | AF7 | RS485 DI |
| **USART3_RX** | PB11 | AF7 | RS485 RO |
| **USART3_DE** | PB1 | AF7 (USART3_RTS_DE) | RS485 DE/RE (HW 자동 제어) |
| **SPI2_SCK** | PB13 | AF5 | W25Q40CLS CLK |
| **SPI2_MISO** | PB14 | AF5 | W25Q40CLS DO |
| **SPI2_MOSI** | PB15 | AF5 | W25Q40CLS DI |
| **FLASH_CS** | PB12 | GPIO_Output, PP, Pull-up, **초기 HIGH** | W25Q40CLS /CS |
| **USB_DM** | PA11 | USB_OTG_FS / USB_DRD_FS | |
| **USB_DP** | PA12 | 〃 | |
| LED_RUN | PA5 | GPIO_Output | 1 Hz 하트비트 |
| LED_WIFI | PB0 | GPIO_Output | WiFi 연결 시 ON |
| LED_ERR | PA6 | GPIO_Output | 에러 표시 |
| SWDIO / SWCLK | PA13 / PA14 | SYS_JTAG-SWD | 디버그 |

> **주의 1** — PB1을 `USART3_RTS_DE`로 잡을 수 없는 실장/AF 조합이면,
> PB1을 일반 GPIO_Output 으로 두고 `App/Inc/app_cfg.h`의
> `SD_RS485_SW_DE` 를 `1` 로 바꾸면 소프트웨어 DE 제어로 동작합니다.
>
> **주의 2** — PB14는 SPI2_MISO와 USART3_DE가 겹치는 핀입니다.
> 위 배치는 SPI2 쪽에 PB14를 할당했으므로 DE는 반드시 PB1을 쓰십시오.
>
> **주의 3** — 핀맵을 바꾸더라도 애플리케이션 코드는 CubeMX가 생성한
> `main.h` 의 `*_Pin` / `*_GPIO_Port` 매크로만 참조하므로, CubeMX의
> **User Label** 을 아래와 같이 지정해 주면 코드 수정이 필요 없습니다.

### 3.1 반드시 지정해야 할 User Label (핀 우클릭 → Enter User Label)

| 핀 | User Label |
|---|---|
| PA4 | `ESP_EN` |
| PA1 | `FPGA_TRIG` |
| PB12 | `FLASH_CS` |
| PB1 | `RS485_DE` (SW DE 모드일 때만 의미 있음) |
| PA5 | `LED_RUN` |
| PB0 | `LED_WIFI` |
| PA6 | `LED_ERR` |

---

## 4. 주변장치 설정

### 4.1 USART1 — ESP32-C WROOM (AT 명령 + 데이터 전송)
| 항목 | 값 |
|---|---|
| Mode | Asynchronous |
| Hardware Flow Control | Disable (RTS/CTS 배선이 있으면 CTS/RTS 권장) |
| Baud Rate | **115200** |
| Word Length | 8 Bits |
| Parity | None |
| Stop Bits | 1 |
| Data Direction | Receive and Transmit |
| Over Sampling | 16 Samples |
| **Advanced → Auto Baudrate** | Disable |
| NVIC | **USART1 global interrupt : Enable, Preemption Priority = 6** |
| DMA Request `USART1_RX` | DMA1 Channel 1, **Peripheral To Memory, Circular**, Byte/Byte, Priority High |
| DMA Request `USART1_TX` | DMA1 Channel 2, Memory To Peripheral, **Normal**, Byte/Byte, Priority Medium |

### 4.2 USART2 — FPGA (Cyclone IV)
| 항목 | 값 |
|---|---|
| Mode | Asynchronous |
| Baud Rate | **115200** |
| 8N1, Receive and Transmit | |
| NVIC | **USART2 global interrupt : Enable, Preemption Priority = 5** |
| DMA `USART2_RX` | DMA1 Channel 3, Peripheral To Memory, **Circular**, Byte/Byte, Priority **Very High** |
| DMA `USART2_TX` | DMA1 Channel 4, Memory To Peripheral, Normal, Byte/Byte, Priority Medium |

### 4.3 USART3 — PC (RS485)
| 항목 | 값 |
|---|---|
| Mode | Asynchronous |
| Baud Rate | **115200** |
| 8N1, Receive and Transmit | |
| **Advanced → Driver Enable** | **Enable** |
| Driver Enable Polarity | High |
| Driver Enable Assertion Time | 8 (1/16 bit 단위) |
| Driver Enable Deassertion Time | 8 |
| NVIC | **USART3 global interrupt : Enable, Preemption Priority = 6** |
| DMA `USART3_RX` | DMA1 Channel 5, Peripheral To Memory, **Circular**, Byte/Byte, Priority Medium |
| DMA `USART3_TX` | DMA1 Channel 6, Memory To Peripheral, Normal, Byte/Byte, Priority Medium |

> `SD_RS485_SW_DE = 1` 로 쓸 경우 Driver Enable은 Disable 하고 PB1을 GPIO_Output 으로.

### 4.4 SPI2 — W25Q40CLS 외부 플래시
| 항목 | 값 |
|---|---|
| Mode | **Full-Duplex Master** |
| NSS | **Disable (Software)** — PB12 GPIO로 직접 제어 |
| Frame Format | Motorola |
| Data Size | 8 Bits |
| First Bit | MSB First |
| **Prescaler** | **/16** → 110 MHz / 16 ≈ 6.875 MHz (W25Q40 최대 104 MHz이나 배선 여유를 둠) |
| Clock Polarity (CPOL) | **Low** |
| Clock Phase (CPHA) | **1 Edge** → SPI Mode 0 |
| CRC Calculation | Disabled |
| NSSP Mode | Disabled |
| DMA | 사용 안 함 (블로킹 전송, 뮤텍스 보호) |
| NVIC | 사용 안 함 |

### 4.5 USB (USB_DRD_FS / USB Device)
| 항목 | 값 |
|---|---|
| Connectivity → USB | **Device (FS)** — Activate_Device 체크 |
| Middleware → USB_DEVICE | Class For FS IP = **Communication Device Class (Virtual Port Com)** |
| VID / PID | 기본값 유지 (또는 자사 값) |
| Product String | `SurgeDetector CDC` |
| CDC Class → `USBD_CDC_RX/TX` 버퍼 | 512 / 512 |
| NVIC | **USB FS global interrupt : Enable, Preemption Priority = 6** |

> **중요** — USB 인터럽트 우선순위는 반드시 `configMAX_SYSCALL_INTERRUPT_PRIORITY`
> 보다 낮은(숫자가 큰) 값이어야 FreeRTOS API 호출이 안전합니다. (5 이상)

### 4.6 GPIO / EXTI
| 항목 | 값 |
|---|---|
| PA1 (FPGA_TRIG) | GPIO_EXTI1, **Falling edge trigger detection**, **Pull-up** |
| NVIC | **EXTI Line1 interrupt : Enable, Preemption Priority = 5** |
| PA4 / PB12 / PA5 / PB0 / PA6 | Output Push-Pull, No pull, Low speed, 초기 상태는 3장 표 참조 |

### 4.7 CRC (선택)
| 항목 | 값 |
|---|---|
| CRC | Activate (기본 파라미터) |

> 본 코드는 소프트웨어 CRC(테이블 없는 비트 연산)를 사용하므로 필수는 아닙니다.
> 하드웨어 CRC를 쓰려면 `util_crc.c` 의 `sd_crc32()` 만 교체하면 됩니다.

### 4.8 IWDG (선택, 권장)
| 항목 | 값 |
|---|---|
| IWDG | Activate |
| Prescaler | 64 |
| Reload | 4095 → 약 8초 |

> `App/Inc/app_cfg.h` 의 `SD_USE_IWDG` 를 1로 하면 하트비트 태스크가 리프레시합니다.

---

## 5. SYS 설정 (매우 중요)

| 항목 | 값 |
|---|---|
| Debug | **Serial Wire** |
| **Timebase Source** | **TIM6** ← 반드시 SysTick 이 아닌 별도 타이머 |

> FreeRTOS가 SysTick을 점유하므로 HAL 타임베이스는 TIM6(또는 TIM7)로 옮겨야 합니다.
> CubeMX가 자동으로 `TIM6 global interrupt` 를 활성화하며 우선순위는 **0** 으로 둡니다.

---

## 6. NVIC 우선순위 정리표

FreeRTOS(CMSIS-RTOS v2, `configPRIO_BITS = 4`, `configMAX_SYSCALL_INTERRUPT_PRIORITY = 5`)
기준입니다. **숫자가 작을수록 높은 우선순위**이며, FreeRTOS API(`...FromISR`)를
호출하는 인터럽트는 반드시 **5 이상**이어야 합니다.

| 인터럽트 | Preemption | Sub | FreeRTOS API 호출 |
|---|---|---|---|
| TIM6 (HAL timebase) | 0 | 0 | X |
| SysTick | 15 | 0 | O (커널) |
| PendSV | 15 | 0 | O (커널) |
| SVCall | 0 | 0 | O (커널) |
| **EXTI Line1 (FPGA_TRIG)** | **5** | 0 | **O** |
| **USART2 (FPGA)** | **5** | 0 | **O** |
| DMA1 Channel3 (USART2_RX) | 5 | 0 | O |
| DMA1 Channel4 (USART2_TX) | 5 | 0 | O |
| **USART1 (ESP32)** | **6** | 0 | **O** |
| DMA1 Channel1/2 (USART1) | 6 | 0 | O |
| **USART3 (RS485)** | **6** | 0 | **O** |
| DMA1 Channel5/6 (USART3) | 6 | 0 | O |
| **USB FS** | **6** | 0 | **O** |

> CubeMX NVIC 탭에서 "Sort by Premption Priority" 로 보면서 위 값을 그대로 입력하십시오.
> **Code generation** 서브탭에서 각 인터럽트의 "Generate IRQ handler" 는 체크 유지.

---

## 7. FreeRTOS 설정

FreeRTOS 상세 설정(태스크/큐/세마포어/뮤텍스 파라미터 전체)은
**[docs/02_FreeRTOS_Design.md](02_FreeRTOS_Design.md)** 에 별도로 정리했습니다.
CubeMX의 Middleware → FREERTOS 탭 값은 그 문서의 표를 그대로 입력하면 됩니다.

---

## 8. Project Manager 탭

### 8.1 Project
| 항목 | 값 |
|---|---|
| Project Name | `SurgeDetector` |
| Toolchain / IDE | STM32CubeIDE |
| Generate Under Root | 체크 |

### 8.2 Code Generator
| 항목 | 값 |
|---|---|
| Copy only the necessary library files | 체크 |
| **Generate peripheral initialization as a pair of '.c/.h' files per peripheral** | **체크** |
| Keep User Code when re-generating | **체크** |
| Delete previously generated files when not re-generated | 체크 해제 (권장) |

### 8.3 Advanced Settings
| 주변장치 | Driver |
|---|---|
| 전체 | HAL |
| USART1/2/3 | HAL, **Do Not Generate Function Call 체크 해제** |
| FREERTOS | 초기화 순서를 GPIO/DMA/USART 뒤로 (기본값이 이미 그러함) |

> **Advanced Settings → Generated Function Calls** 에서 `MX_USB_Device_Init` 은
> 호출 위치를 확인하십시오. FreeRTOS 사용 시 USB 초기화는
> `MX_FREERTOS_Init()` 이전(main 함수 내부)에 두는 기본 배치로 충분합니다.

---

## 9. 코드 생성 후 해야 할 일

1. `App/` 폴더를 프로젝트 루트에 복사
2. STM32CubeIDE → 프로젝트 우클릭 → Properties → C/C++ General → Paths and Symbols
   - **Includes → GNU C** 에 `../App/Inc` 추가
   - **Source Location** 에 `/SurgeDetector/App` 추가
3. `Integration/main_usercode.c` 의 스니펫을 `Core/Src/main.c` 의 USER CODE 구간에 반영
4. `Integration/usbd_cdc_if_usercode.c` 의 스니펫을 `USB_DEVICE/App/usbd_cdc_if.c` 에 반영
5. `Integration/stm32l5xx_it_usercode.c` 확인 (기본 생성 코드로 충분, 참고용)
6. 빌드 → `App/Src/*.c` 가 모두 컴파일되는지 확인

---

## 10. 링커 / 힙 설정

STM32CubeIDE 기본 `STM32L562CETX_FLASH.ld` 사용.
FreeRTOS heap_4 (`configTOTAL_HEAP_SIZE = 40960`)를 쓰므로 시스템 힙(`_Min_Heap_Size`)은
기본 0x200 그대로 두어도 됩니다. 시스템 스택(`_Min_Stack_Size`)은 **0x600** 이상 권장
(인터럽트 컨텍스트에서 사용).

| 리소스 | 사용량(대략) |
|---|---|
| SRAM | 256 KB (SRAM1 192KB + SRAM2 64KB) |
| FreeRTOS heap | 40 KB |
| DMA/링버퍼 정적 | 약 3 KB |
| USB CDC 버퍼 | 약 2 KB |
