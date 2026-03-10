# STM32CubeMX 설정 가이드 - STM32H563 RAM 구성

## 프로젝트 생성

### Step 1: 디바이스 선택

```
STM32CubeMX 실행
→ File → New Project
→ MCU/MPU Selector 탭
→ 검색: STM32H563ZI
→ STM32H563ZITx 선택 (Nucleo-H563ZI 기준)
→ Start Project
```

---

## 주변장치 설정 (Pinout & Configuration 탭)

### Step 2: RCC 설정

```
System Core → RCC
├── High Speed Clock (HSE): Crystal/Ceramic Resonator
│   (Nucleo 보드: BYPASS Clock Source)
└── Low Speed Clock (LSE): Crystal/Ceramic Resonator
    (BKPRAM용 백업 도메인 클럭)
```

### Step 3: SYS 설정

```
System Core → SYS
├── Debug: Serial Wire (SWD)
└── Timebase Source: SysTick
```

### Step 4: PWR 설정 (BKPRAM 사용 필수)

```
System Core → PWR
├── [✓] Backup Regulator          ← VBAT 도메인 전압 레귤레이터
├── [✓] Backup Domain Voltage     ← VBAT 독립 전원
└── Power Regulator Voltage Scale: Scale 0 (최고 성능, 250MHz)
```

### Step 5: Cortex-M33 설정 (MPU)

```
System Core → CORTEX_M33
├── Cortex Interface Settings:
│   ├── [✓] CPU ICache
│   └── [✓] CPU DCache
└── MPU Settings:
    ├── [✓] MPU Control Mode: Enable with PRIVDEFENA
    ├── Region 0 (SRAM2 - Non-Cacheable):
    │   ├── Enable: Enabled
    │   ├── Base Address: 0x20040000
    │   ├── Size: 64KB
    │   ├── SubRegion Disable: 0x00
    │   ├── TEX field level: Level 0
    │   ├── Access Permission: ALL ACCESS PERMITTED
    │   ├── Instruction Access: DISABLE
    │   ├── Shareable: Enabled         ← DMA 공유
    │   ├── Cacheable: Disabled        ← 캐시 비활성화
    │   └── Bufferable: Disabled
    └── Region 1 (SRAM1 - Cacheable):
        ├── Enable: Enabled
        ├── Base Address: 0x20000000
        ├── Size: 256KB
        ├── TEX field level: Level 1   ← Write-Back Write-Allocate
        ├── Access Permission: ALL ACCESS PERMITTED
        ├── Instruction Access: ENABLE
        ├── Shareable: Disabled
        ├── Cacheable: Enabled
        └── Bufferable: Enabled
```

### Step 6: GPDMA 설정

```
System Core → GPDMA1
└── Channel 0:
    ├── [✓] Enable
    ├── Request: Software (메모리-메모리 전송)
    ├── Direction: Memory To Memory
    ├── Source Data Width: Word (32-bit)
    ├── Destination Data Width: Word (32-bit)
    ├── Source Increment: Enabled
    └── Destination Increment: Enabled
```

### Step 7: UART3 설정 (ST-Link VCP)

```
Connectivity → USART3
├── Mode: Asynchronous
└── Configuration:
    ├── Baud Rate: 115200
    ├── Word Length: 8 Bits
    ├── Parity: None
    ├── Stop Bits: 1
    └── DMA Settings:
        ├── Add → DMA Request: USART3_RX
        │   └── Direction: Peripheral To Memory
        │       Destination: uartRxDmaBuf (SRAM2)
        └── Add → DMA Request: USART3_TX
            └── Direction: Memory To Peripheral
                Source: uartTxDmaBuf (SRAM2)
```

---

## 클럭 설정 (Clock Configuration 탭)

```
HSE Input: 8 MHz (Nucleo HSE Bypass)
PLL Source: HSE

PLL1 설정:
├── DIVM1: /4     → PLL 입력: 2 MHz
├── MULN1: ×250   → VCO: 500 MHz
├── DIVP1: /2     → PLL1P: 250 MHz → SYSCLK
├── DIVQ1: /2     → PLL1Q: 250 MHz
└── DIVR1: /2     → PLL1R: 250 MHz

시스템 클럭 트리:
SYSCLK = 250 MHz
HCLK   = 250 MHz (AHB Prescaler: /1)
PCLK1  = 125 MHz (APB1 Prescaler: /2)
PCLK2  = 125 MHz (APB2 Prescaler: /2)
PCLK3  = 125 MHz (APB3 Prescaler: /2)
```

---

## 프로젝트 설정 (Project Manager 탭)

### Step 8: 기본 프로젝트 설정

```
Project Manager → Project
├── Project Name: STM32H5_RAM_Config
├── Project Location: C:\Users\...\STM32CubeIDE\workspace
├── Application Structure: Basic
├── Toolchain/IDE: STM32CubeIDE (또는 Makefile/EWARM)
└── Linker Settings:
    └── Minimum Heap Size: 0x800
    └── Minimum Stack Size: 0x1000
```

### Step 9: 코드 생성 설정

```
Project Manager → Code Generator
├── [✓] Copy only the necessary library files
├── [✓] Generate peripheral initialization as a pair of '.c/.h' files
└── Generated files options:
    └── [✓] Keep User Code when re-generating
```

### Step 10: 코드 생성

```
우상단 [GENERATE CODE] 버튼 클릭
→ STM32CubeIDE에서 프로젝트 열기
```

---

## 코드 생성 후 수동 설정

### 링커 스크립트 수정

CubeMX가 생성한 `STM32H563ZITX_FLASH.ld`를 열어 커스텀 섹션 추가:

```bash
# 프로젝트 내 링커 스크립트 위치
STM32H5_RAM_Config/STM32H563ZITX_FLASH.ld
```

**추가할 내용** (MEMORY 블록 수정):

```ld
/* 기존 CubeMX 생성 내용에 SRAM2, BKPRAM 추가 */
MEMORY
{
  FLASH   (rx)  : ORIGIN = 0x08000000, LENGTH = 2048K
  RAM     (xrw) : ORIGIN = 0x20000000, LENGTH = 256K   /* SRAM1 */
  SRAM2   (rwx) : ORIGIN = 0x20040000, LENGTH = 64K    /* 추가 */
  BKPRAM  (rw)  : ORIGIN = 0x40003C00, LENGTH = 4K     /* 추가 */
}
```

**SECTIONS 블록에 추가** (`.bss` 섹션 다음에 삽입):

```ld
/* SRAM2 데이터 섹션 */
_sisram2 = LOADADDR(.sram2_data);
.sram2_data :
{
  . = ALIGN(32);
  _ssram2 = .;
  *(.sram2_data)
  *(.sram2_data*)
  . = ALIGN(32);
  _esram2 = .;
} >SRAM2 AT> FLASH

/* SRAM2 BSS 섹션 (초기화 없음) */
.sram2_bss (NOLOAD) :
{
  . = ALIGN(32);
  _ssram2_bss = .;
  *(.sram2_bss)
  *(.sram2_bss*)
  . = ALIGN(32);
  _esram2_bss = .;
} >SRAM2

/* BKPRAM 섹션 (NOLOAD - 리셋 후에도 데이터 유지) */
.bkpram_data (NOLOAD) :
{
  . = ALIGN(4);
  _sbkpram = .;
  *(.bkpram_data)
  *(.bkpram_data*)
  . = ALIGN(4);
  _ebkpram = .;
} >BKPRAM
```

### startup 파일 수정 (선택사항)

`startup_stm32h563xx.s` 또는 `.c`에서 SRAM2 초기화를 추가할 수 있지만,
`SRAM2_Init()` 함수를 main에서 호출하는 방법이 더 간단합니다.

### STM32CubeIDE 빌드 설정 확인

```
프로젝트 우클릭 → Properties
→ C/C++ Build → Settings
→ MCU GCC Linker → General
→ Linker Script: ${workspace_loc:/${ProjName}/STM32H563ZITX_FLASH.ld}
```

---

## 빌드 및 검증

### 메모리 맵 확인

```bash
# .map 파일에서 섹션 배치 확인
# 위치: Debug/STM32H5_RAM_Config.map

# 또는 arm-none-eabi-objdump 사용
arm-none-eabi-objdump -h STM32H5_RAM_Config.elf

# 예상 출력:
# .data    SRAM1  0x20000000  (초기화된 전역변수)
# .bss     SRAM1  0x20001234  (미초기화 전역변수)
# .sram2_data  SRAM2  0x20040000  (SRAM2 데이터)
# .sram2_bss   SRAM2  0x20040100  (SRAM2 BSS)
# .bkpram_data BKPRAM 0x40003C00  (백업 RAM)
```

### 변수 주소 확인 (디버거)

STM32CubeIDE 디버거에서 Memory 뷰를 사용하여 확인:

```
Window → Show View → Memory Browser

확인 주소:
  0x20000000 - SRAM1 시작 (스택, BSS)
  0x20040000 - SRAM2 시작 (DMA 버퍼)
  0x40003C00 - BKPRAM 시작 (백업 데이터)
```

---

## TrustZone 설정 (STM32H573 전용)

STM32H573은 TrustZone을 지원합니다. CubeMX에서 활성화 가능:

```
System Core → GTZC1
└── Memory Protection:
    ├── SRAM1:
    │   ├── 0x20000000 ~ 0x2002FFFF: Non-Secure (192KB)
    │   └── 0x20030000 ~ 0x2003FFFF: Secure (64KB)
    └── SRAM2:
        └── 0x20040000 ~ 0x2004FFFF: Non-Secure (64KB)
```

**주의**: TrustZone 활성화 시 Secure/Non-Secure 코드 분리 필요.
일반 개발에서는 TrustZone 비활성화 상태(STM32H563)를 권장합니다.
