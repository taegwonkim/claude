# STM32CubeMX 설정 가이드 (v2 – MPU 비캐시 방식)
# STM32H563ZI – SD 카드(SDMMC1 IDMA) + USART3(GPDMA1) 충돌 없는 구성

대상 보드: **NUCLEO-H563ZI** (STM32H563ZI, Cortex-M33, 250 MHz)

---

## 1. 왜 SDMMC는 IDMA이고 GPDMA는 UART에 쓰나?

```
STM32H5 DMA 구조:
┌──────────────┐    ┌────────────────────────────────┐
│ SDMMC1 IDMA  │    │ GPDMA1 (8ch) / GPDMA2 (8ch)    │
│ (내장 DMA)    │    │ 연결 가능 주변장치:             │
│ AHB Master   │    │   USART1/2/3, SPI, I2C, ADC... │
│ 전용 데이터   │    │   SDMMC는 목록 없음 (IDMA 전용) │
│ 경로 존재     │    └────────────────────────────────┘
└──────────────┘
```

STM32H5 SDMMC1은 GPDMA 요청 라인이 없습니다.
SDMMC 전용 IDMA(내부 DMA)만 사용 가능합니다.
따라서 UART를 GPDMA에 연결해 두 전송이 서로 다른 엔진에서 동시에 동작합니다.

---

## 2. 프로젝트 생성

| 항목 | 값 |
|------|-----|
| MCU | STM32H563ZI |
| Project Name | STM32H5_SD_GPDMA |
| Toolchain | STM32CubeIDE (GCC) |
| Heap | 0x2000 (8 KB) |
| Stack | 0x1000 (4 KB) |

---

## 3. 클럭 설정

```
HSE  : 8 MHz (NUCLEO bypass 모드)
PLL1 :
  PLLM   = 1
  PLLN   = 125
  PLLP/2 → SYSCLK = 250 MHz
  PLLQ/10→ PLL1Q  = 100 MHz ← SDMMC1 커널 클럭

HCLK  = 250 MHz  (AHB Prescaler /1)
PCLK1 = 125 MHz  (APB1 /2) ← USART3 소속
Flash Latency = 5 WS  (250 MHz, VOS0)
VOS   = VOS0 (최고 성능 설정)

RCC PeriphClock:
  SDMMC1 Clock Source → PLL1Q (100 MHz)
```

---

## 4. Cortex-M33 – MPU 설정 ★핵심★

> **캐시 일관성을 MPU로 해결**합니다.
> DMA 버퍼 전용 SRAM 영역을 Non-Cacheable로 지정하면
> `SCB_CleanDCache` / `SCB_InvalidateDCache` 호출이 불필요합니다.

**System Core → Cortex_M33**

| 파라미터 | 값 |
|----------|-----|
| MPU Control | Enable |
| MPU Region 0 | Enable |

**Region 0 설정 – DMA 버퍼 (SRAM2 시작, 8 KB)**

| 필드 | 값 | 설명 |
|------|----|------|
| Base Address | 0x20030000 | SRAM2 시작 주소 |
| Size | 8 KB | sd_buf(2KB) + uart_buf(512B) + 여유 |
| Access Permission | Full Access | R/W |
| TEX | 1 | Normal Memory |
| Cacheable | No | 캐시 비활성화 |
| Bufferable | No | 버퍼 비활성화 |
| Shareable | Yes | DMA 공유 |
| Instruction Access | Disable | 코드 실행 불가 |

> 코드에서 버퍼를 SRAM2(0x20030000)에 배치:
> ```c
> static uint8_t sd_buf[2048] __attribute__((section(".dma_buf")));
> ```
> 링커 스크립트에서 `.dma_buf` 섹션을 0x20030000에 할당.

---

## 5. SDMMC1 설정

**Connectivity → SDMMC1**

| 파라미터 | 값 |
|----------|-----|
| Mode | SD 4 bits Wide bus |
| Clock Edge | Rising |
| Clock Power Save | Disable |
| Bus Wide | 4 bits |
| HW Flow Control | Disable |
| Clock Div | 4 (→ SDMMC_CK = 12.5 MHz, DS 모드) |

> HS 모드(25 MHz)는 `HAL_SD_ConfigSpeedBusOperation(HS)` 로 런타임 전환.

**핀 배정 (NUCLEO-H563ZI)**

| 핀 | 기능 | AF |
|----|------|----|
| PC8  | SDMMC1_D0 | AF12 |
| PC9  | SDMMC1_D1 | AF12 |
| PC10 | SDMMC1_D2 | AF12 |
| PC11 | SDMMC1_D3 | AF12 |
| PC12 | SDMMC1_CK | AF12 |
| PD2  | SDMMC1_CMD | AF12 |

**NVIC**

| 인터럽트 | Preempt | Sub | 비고 |
|----------|---------|-----|------|
| SDMMC1 global | **4** | 0 | IDMA 완료 – 최우선 |

---

## 6. USART3 설정

**Connectivity → USART3**

| 파라미터 | 값 |
|----------|-----|
| Mode | Asynchronous |
| Baud Rate | 115200 |
| Data Bits | 8 |
| Stop Bits | 1 |
| Parity | None |
| DMA TX | GPDMA1 Channel 0 |

**핀 배정**

| 핀 | 기능 | AF |
|----|------|----|
| PD8 | USART3_TX | AF7 |
| PD9 | USART3_RX | AF7 |

---

## 7. GPDMA1 설정

**System Core → GPDMA1 → Channel 0**

| 파라미터 | 값 |
|----------|-----|
| Request | USART3_TX |
| Direction | Memory to Peripheral |
| Src Increment | Enable |
| Dst Increment | Disable |
| Src Width | Byte |
| Dst Width | Byte |
| Priority | Low |
| Mode | Normal (단방향, 완료 후 정지) |
| Src Burst | 1 |
| Dst Burst | 1 |
| Src Port | Port 0 |
| Dst Port | Port 1 |

**NVIC**

| 인터럽트 | Preempt | Sub |
|----------|---------|-----|
| GPDMA1 Channel 0 | **6** | 0 |
| USART3 global | **6** | 0 |

---

## 8. GPIO – LED

**GPIO → Output Push-Pull**

| 핀 | Label | 초기값 |
|----|-------|-------|
| PB0 | LED_GREEN  | Low |
| PF4 | LED_YELLOW | Low |
| PG4 | LED_RED    | Low |

---

## 9. NVIC 우선순위 전체 요약

```
우선순위 번호 (낮을수록 높음):
  4   SDMMC1      SD IDMA 완료 → 빠른 응답 필수
  6   GPDMA1_CH0  USART3 TX 완료
  6   USART3      UART 오류
 15   SysTick     HAL 틱 (HAL_Init이 자동 설정)
```

---

## 10. 링커 스크립트 – DMA 버퍼 섹션

**STM32H563ZI_FLASH.ld** 수정:

```ld
/* ① SRAM2를 별도 메모리 영역으로 선언 (MPU Region 0과 일치) */
MEMORY
{
  FLASH  (rx)  : ORIGIN = 0x08000000, LENGTH = 2048K
  RAM    (xrw) : ORIGIN = 0x20000000, LENGTH = 192K   /* SRAM1 */
  RAM2   (xrw) : ORIGIN = 0x20030000, LENGTH = 64K    /* SRAM2 – DMA 전용 */
}

SECTIONS
{
  /* ... 기존 섹션들 ... */

  /* ② DMA 버퍼 전용 섹션 → SRAM2 배치 (MPU Non-Cacheable) */
  .dma_buf (NOLOAD) :
  {
    . = ALIGN(32);
    *(.dma_buf)
    *(.dma_buf*)
    . = ALIGN(32);
  } >RAM2
}
```

---

## 11. 충돌 방지 메커니즘 요약

| 문제 | 해결책 |
|------|--------|
| D-Cache vs DMA 불일치 | MPU Region 0 → Non-Cacheable (SCB 호출 불필요) |
| UART 블로킹 → SD 타임아웃 | GPDMA1 비동기 TX + 링 버퍼 |
| SD IRQ vs UART IRQ 우선순위 | SDMMC(4) > GPDMA(6) |
| 두 DMA 같은 메모리 충돌 | sd_buf ≠ uart_tx_buf (완전 분리) |
| AHB 버스 포화 | SDMMC IDMA + GPDMA1이 AHB arbiter로 중재 |
