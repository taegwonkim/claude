# STM32H5 RAM 구성 가이드

## 목차
1. [STM32H5 메모리 구조 개요](#1-stm32h5-메모리-구조-개요)
2. [RAM 영역별 특성](#2-ram-영역별-특성)
3. [STM32CubeMX 설정](#3-stm32cubemx-설정)
4. [링커 스크립트 구성](#4-링커-스크립트-구성)
5. [예제 코드](#5-예제-코드)
6. [주의사항 및 팁](#6-주의사항-및-팁)

---

## 1. STM32H5 메모리 구조 개요

STM32H5 시리즈는 Cortex-M33 코어 기반으로, 여러 개의 독립적인 RAM 영역을 제공합니다.

### 디바이스별 RAM 용량

| 디바이스        | SRAM1   | SRAM2  | BKPRAM | 총 RAM    |
|----------------|---------|--------|--------|-----------|
| STM32H563/573  | 256 KB  | 64 KB  | 4 KB   | 324 KB    |
| STM32H562      | 256 KB  | 64 KB  | 4 KB   | 324 KB    |
| STM32H503      | 32 KB   | -      | 2 KB   | 34 KB     |

### 메모리 맵 (STM32H563/573 기준)

```
0x2000_0000 ┌─────────────────┐
            │   SRAM1 (256KB) │  ← 일반 목적 RAM (스택, 힙, 데이터)
0x2004_0000 ├─────────────────┤
            │   SRAM2 (64KB)  │  ← 고속 DMA, 보안 데이터
0x2005_0000 ├─────────────────┤
            │    (예약)        │
0x4003_C000 ├─────────────────┤
            │  BKPRAM (4KB)   │  ← 배터리 백업 RAM
0x4003_D000 └─────────────────┘
```

---

## 2. RAM 영역별 특성

### 2.1 SRAM1 (주 RAM)

- **주소**: `0x20000000` ~ `0x2003FFFF` (256KB)
- **버스**: AHB (Advanced High-performance Bus)
- **특성**:
  - 일반적인 코드 실행 및 데이터 저장에 사용
  - 스택(Stack) 및 힙(Heap) 기본 위치
  - 전원 공급 중단 시 데이터 손실
  - DMA 접근 가능

```c
/* SRAM1에 변수 배치 예시 */
// 기본적으로 모든 전역변수는 SRAM1에 위치
uint32_t normalArray[100];   // .bss 또는 .data 섹션 → SRAM1

/* 명시적으로 SRAM1 섹션 지정 */
__attribute__((section(".sram1_data")))
uint32_t sram1Buffer[256];
```

### 2.2 SRAM2 (보조 RAM)

- **주소**: `0x20040000` ~ `0x2004FFFF` (64KB)
- **버스**: AHB
- **특성**:
  - 고속 DMA 작업에 최적
  - TrustZone 보안 설정 가능 (STM32H573)
  - 독립적인 전원 도메인 없음 (SRAM1과 동일한 전원)
  - 일부 peripheral은 특정 SRAM 영역만 접근 가능

```c
/* SRAM2에 DMA 버퍼 배치 */
__attribute__((section(".sram2_data")))
__attribute__((aligned(32)))   // 캐시 라인 정렬
uint8_t dmaBuffer[1024];
```

### 2.3 BKPRAM (배터리 백업 RAM)

- **주소**: `0x40003C00` ~ `0x40003FFF` (4KB, STM32H563)
- **버스**: APB (LSE/VBAT 도메인)
- **특성**:
  - VBAT 핀으로 배터리 전원 공급 시 VDD 차단 후에도 데이터 유지
  - RTC와 같은 도메인에 위치
  - 주의: 일반 RAM처럼 DMA 불가
  - RCC_BDCR 레지스터로 접근 활성화 필요

```c
/* BKPRAM 접근 활성화 및 사용 */
void BKPRAM_Init(void)
{
    /* PWR 클럭 활성화 */
    __HAL_RCC_PWR_CLK_ENABLE();

    /* 백업 도메인 쓰기 보호 해제 */
    HAL_PWR_EnableBkUpAccess();

    /* BKPRAM 클럭 활성화 */
    __HAL_RCC_BKPRAM_CLK_ENABLE();
}

/* BKPRAM 섹션에 변수 배치 */
__attribute__((section(".bkpram_data")))
uint32_t backupData[10];   /* VBAT 공급 시 전원 차단 후에도 유지 */
```

---

## 3. STM32CubeMX 설정

### 3.1 기본 설정 순서

```
1. STM32CubeMX 실행
2. File → New Project → STM32H563xx 선택
3. Pinout & Configuration 탭에서 주변장치 설정
4. Project Manager 탭에서 프로젝트 설정
5. Code Generation 클릭
```

### 3.2 BKPRAM 활성화 (CubeMX)

```
Pinout & Configuration
  → System Core
    → PWR
      ☑ Backup Regulator (VBAT 사용 시)
      ☑ Backup SRAM (BKPRAM 사용 시)

  → System Core
    → RCC
      → Backup Domain Clock Source: LSE 또는 LSI 선택
```

### 3.3 MPU (Memory Protection Unit) 설정

TrustZone 비활성화 환경에서도 MPU로 메모리 보호 가능:

```
Pinout & Configuration
  → System Core
    → CORTEX_M33
      → MPU Control Mode: Enable

      MPU Region 0: SRAM1
        - Start Address: 0x20000000
        - Size: 256KB
        - Access Permission: Full Access
        - Instruction Access: Enable
        - Cacheable: Enable
        - Bufferable: Enable
        - Shareable: Disable

      MPU Region 1: SRAM2 (DMA 전용)
        - Start Address: 0x20040000
        - Size: 64KB
        - Access Permission: Full Access
        - Instruction Access: Disable
        - Cacheable: Disable      ← DMA 사용 시 비캐시화
        - Bufferable: Disable
        - Shareable: Enable       ← DMA 공유
```

### 3.4 DMA 설정 (GPDMA)

STM32H5는 GPDMA(General Purpose DMA)를 사용합니다:

```
Pinout & Configuration
  → System Core
    → GPDMA1
      → Channel 0 활성화
        - Request: 사용할 Peripheral 선택
        - Direction: Peripheral To Memory 또는 Memory To Memory
        - Source Address: Peripheral 주소
        - Destination Address: SRAM2 주소 (DMA 버퍼)
```

### 3.5 링커 스크립트 커스터마이징 (CubeMX 이후)

CubeMX가 생성한 링커 스크립트(`STM32H563ZITX_FLASH.ld`)를 수정하여 커스텀 섹션 추가:

```
Project Manager 탭
  → Advanced Settings
    → Linker Settings: 생성된 .ld 파일 위치 확인
```

---

## 4. 링커 스크립트 구성

파일 위치: `STM32H563ZITX_FLASH.ld` (또는 프로젝트에 맞는 이름)

전체 링커 스크립트는 [`linker/STM32H563ZITX_FLASH.ld`](linker/STM32H563ZITX_FLASH.ld) 참조.

### 핵심 메모리 정의

```ld
MEMORY
{
  FLASH   (rx)  : ORIGIN = 0x08000000, LENGTH = 2048K
  SRAM1   (rwx) : ORIGIN = 0x20000000, LENGTH = 256K
  SRAM2   (rwx) : ORIGIN = 0x20040000, LENGTH = 64K
  BKPRAM  (rw)  : ORIGIN = 0x40003C00, LENGTH = 4K
}
```

---

## 5. 예제 코드

예제 파일 목록:

| 파일 | 설명 |
|------|------|
| [`src/main.c`](src/main.c) | 메인 통합 예제 |
| [`src/ram_config.c`](src/ram_config.c) | RAM 초기화 및 관리 |
| [`src/ram_config.h`](src/ram_config.h) | RAM 설정 헤더 |
| [`src/bkpram_example.c`](src/bkpram_example.c) | BKPRAM 백업/복원 예제 |
| [`src/dma_sram2_example.c`](src/dma_sram2_example.c) | SRAM2 DMA 전송 예제 |
| [`linker/STM32H563ZITX_FLASH.ld`](linker/STM32H563ZITX_FLASH.ld) | 커스텀 링커 스크립트 |

---

## 6. 주의사항 및 팁

### 캐시 일관성 (Cache Coherency)

STM32H5는 I-Cache와 D-Cache를 지원합니다. DMA와 캐시를 함께 사용할 때 반드시 캐시 관리를 해야 합니다:

```c
/* DMA TX 전 캐시 플러시 (CPU → DMA) */
SCB_CleanDCache_by_Addr((uint32_t*)dmaBuffer, sizeof(dmaBuffer));

/* DMA RX 후 캐시 무효화 (DMA → CPU) */
SCB_InvalidateDCache_by_Addr((uint32_t*)dmaBuffer, sizeof(dmaBuffer));
```

**권장 해결책**: DMA 버퍼를 SRAM2에 배치하고 해당 영역을 Non-Cacheable로 MPU 설정.

### 메모리 정렬

- DMA 버퍼는 최소 **4바이트** 정렬 필요
- D-Cache 사용 시 **32바이트 (캐시 라인)** 정렬 권장

```c
/* 올바른 DMA 버퍼 선언 */
__attribute__((section(".sram2_data")))
__attribute__((aligned(32)))
uint8_t dmaRxBuf[256];
```

### BKPRAM 사용 시 주의

1. `HAL_PWR_EnableBkUpAccess()` 호출 후에만 쓰기 가능
2. VBAT 핀에 배터리 또는 슈퍼캐패시터 연결 필요
3. 전원 초기화 순서: PWR 클럭 → 백업 접근 허용 → BKPRAM 클럭 → 읽기/쓰기

### TrustZone (STM32H573)

STM32H573은 TrustZone을 지원합니다. SRAM 영역을 Secure/Non-Secure로 분리 가능:

```
GTZC (Global TrustZone Controller)에서 설정:
- SRAM1: 하위 192KB = Non-Secure, 상위 64KB = Secure
- SRAM2: 전체 Non-Secure 또는 전체 Secure
```
