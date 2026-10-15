# STM32H7 + Microchip LAN8770 (EV48S68A, 100BASE-T1) — STM32CubeMX / CubeIDE 프로젝트

STM32H7 내장 Ethernet MAC 과 **Microchip EVB-LAN8770-RMII (EV48S68A)** 를
**RMII + MDIO/MDC** 로 연결하고, lwIP(NO_SYS, 베어메탈) 위에서
**ping 응답 + UDP 에코 서버**가 동작하도록 만든 코드입니다.

- 대상 MCU : **STM32H743ZI** (NUCLEO-H743ZI2 기준 핀맵). 다른 H74x/H75x/H72x/H73x 도 같은 방식
- PHY : **LAN8770** (100BASE-T1, 오토네고 없음, 100 Mbps Full Duplex 고정)
- 스택 : STM32CubeMX 의 **LwIP 미들웨어** (STM32CubeH7 FW 1.10 이상의 새 ETH HAL API)
- 기능
  - LAN8770 드라이버 : ID 확인/주소 자동 탐색, 소프트 리셋, Microchip 권장 초기화 시퀀스,
    **MASTER/SLAVE 설정**, 링크 상태, **SQI(신호 품질 0~7)**, 인터럽트, 루프백
  - PHY 를 못 찾으면 멈추지 않고 1초마다 재시도 (EVB 전원이 늦게 들어와도 복구)
  - UART 로그 : PHY 주소·역할·링크·SQI·IP 를 2초마다 출력
  - UDP 에코(포트 7) + PC 테스트 스크립트

```
 STM32H743 (MAC)          EVB-LAN8770-RMII (EV48S68A)        상대편 T1 장비
┌──────────────┐  RMII   ┌──────────────────────┐ 100BASE-T1  ┌──────────────┐
│ ETH MAC  DMA │═════════│ LAN8770              │─────────────│ T1 미디어컨버터│── PC
│              │ MDIO/MDC│  (MASTER or SLAVE)   │ 1 pair UTP  │ / LAN8770 EVB │
└──────────────┘─────────└──────────────────────┘             └──────────────┘
```

---

## 1. 하드웨어 연결

### 1-1. RMII 배선 (STM32H743ZI, NUCLEO-H743ZI2 의 표준 ETH 핀)

| 신호 | STM32 핀 | AF | 방향 (MCU 기준) | 비고 |
|------|---------|----|----------------|------|
| REF_CLK | **PA1** | AF11 | 입력 | 50 MHz. STM32 는 항상 **입력**으로 받음 |
| MDIO | **PA2** | AF11 | 양방향 | 1.5 kΩ 풀업 필요 (EVB 에 있는지 확인) |
| CRS_DV | **PA7** | AF11 | 입력 | |
| MDC | **PC1** | AF11 | 출력 | ≤ 2.5 MHz (HAL 이 자동 분주) |
| RXD0 | **PC4** | AF11 | 입력 | |
| RXD1 | **PC5** | AF11 | 입력 | |
| TX_EN | **PG11** | AF11 | 출력 | |
| TXD0 | **PG13** | AF11 | 출력 | |
| TXD1 | **PB13** | AF11 | 출력 | |
| nRESET | 아무 GPIO (예: **PG0**) | GPIO | 출력 | CubeMX User Label **`PHY_RST`** 로 하면 자동 사용 |
| nINT | 아무 GPIO | EXTI | 입력 | 선택 사항 (이 예제는 폴링) |
| GND | GND | | | **여러 가닥** |
| 전원 | 3.3 V / 5 V | | | EVB 가 요구하는 레일을 EVB 사용자 가이드로 확인 |

> EVB 의 RMII 커넥터는 Microchip SAMA5D3 EDS 호스트 규격입니다.
> **커넥터 핀 번호는 반드시 EVB 회로도(사용자 가이드)로 확인**하세요.

### 1-2. REF_CLK (50 MHz) — 가장 먼저 확인할 것

STM32 RMII 는 REF_CLK 를 **만들지 않고 받기만** 합니다. 다음 중 하나여야 합니다.

| 방식 | 설명 |
|------|------|
| **A. PHY 가 출력** | LAN8770 이 25 MHz 크리스탈로 50 MHz 를 만들어 REF_CLK 핀으로 출력 (strap 설정). → PA1 에 그대로 연결. EVB 기본 설정을 회로도로 확인 |
| B. 외부 50 MHz 오실레이터 | 오실레이터 출력을 PHY REF_CLK 입력과 PA1 에 함께 연결 |
| C. STM32 MCO 출력 | MCO1(PA8)로 50 MHz 를 만들어 PHY 와 PA1 에 함께 연결 (지터 주의) |

REF_CLK 가 없으면 `HAL_ETH_Init()` 의 DMA 소프트 리셋이 끝나지 않아 `Error_Handler()` 로 빠집니다.

### 1-3. ⚠ NUCLEO-H743ZI2 를 쓸 때

보드에 있는 **LAN8742 PHY 가 위 RMII 핀에 이미 연결**되어 있습니다.
특히 PA1/PA7/PC4/PC5 는 대체 핀이 없습니다. 그래서 **UM2407(보드 매뉴얼)의 Ethernet 절을 보고
해당 솔더 브리지를 제거해 온보드 PHY 를 분리**해야 합니다.
분리하지 않으면 두 PHY 가 RXD/CRS_DV 를 동시에 구동해 통신이 깨집니다.
(드라이버는 PHY ID 로 LAN8770 만 골라내므로 MDIO 는 공유돼도 괜찮습니다.)

### 1-4. 배선 품질

- 50 MHz 신호입니다. 점퍼선은 **10 cm 이하**, 가능하면 어댑터 PCB 를 권장합니다
- GPIO 속도는 **Very High** (ethernetif.c 의 `HAL_ETH_MspInit` 에서 설정)
- GND 를 신호선 사이사이에 함께 묶을수록 좋습니다

### 1-5. 상대편 (100BASE-T1 은 일반 PC 랜카드에 바로 연결되지 않습니다)

- **100BASE-T1 ↔ 100BASE-TX 미디어 컨버터** → PC (가장 편함)
- 또는 LAN8770 EVB 한 장 더 / 다른 T1 장비
- 링크 양 끝 중 **하나는 MASTER, 하나는 SLAVE** 여야 합니다 → `lan8770_conf.h` 의 `APP_PHY_ROLE`

---

## 2. 파일 구성

```
STM32H7_LAN8770_Ethernet/
├── Core/
│   ├── Inc/
│   │   ├── lan8770_conf.h        ← 사용자 설정 (PHY 주소/역할, MAC, 포트, 로그)
│   │   └── app_net.h
│   └── Src/
│       └── app_net.c             ← UDP 에코 서버 + 링크/SQI 상태 로그
├── LWIP/Target/
│   ├── ethernetif.c              ← CubeMX 생성본을 이 파일로 "교체" (LAN8742 → LAN8770)
│   └── ethernetif.h
├── Drivers/BSP/Components/lan8770/
│   ├── lan8770.c                 ← PHY 드라이버 (ST BSP Component 스타일)
│   └── lan8770.h
├── linker/
│   └── lwip_sec_snippet.ld       ← 링커 스크립트에 넣을 D2 SRAM 섹션
└── tools/
    ├── install_to_cubeide.sh     ← 위 파일들을 CubeIDE 프로젝트에 복사 + 링커 패치
    └── udp_echo_test.py          ← PC 쪽 UDP 에코 테스트
```

### 계층 구조

```
main.c ──> MX_LWIP_Init() / MX_LWIP_Process()      (CubeMX 생성 LWIP/App/lwip.c)
   │               │
   │               └─> ethernetif.c  ── HAL ETH (RMII, DMA) ──> STM32 ETH MAC
   │                        │
   │                        └─> lan8770.c ── HAL_ETH_Read/WritePHYRegister (MDIO)
   └──> app_net.c (UDP 에코, 상태 로그)
```

---

## 3. 메모리 배치 — STM32H7 에서 가장 많이 막히는 부분

ETH DMA 는 **DTCM(0x2000_0000)에 접근할 수 없습니다.** 그래서 디스크립터와 버퍼를 **D2 SRAM** 에 두고,
D-Cache 와의 일관성 문제를 피하려고 **MPU 로 non-cacheable** 로 만듭니다.

| 주소 | 내용 | 크기 | 배치 방법 |
|------|------|------|-----------|
| 0x3000_0000 | Rx DMA 디스크립터 | 4 × 24 B | `.RxDescripSection` (링커) |
| 0x3000_0080 | Tx DMA 디스크립터 | 4 × 24 B | `.TxDescripSection` (링커) |
| 0x3000_0100 | Rx 버퍼 풀 (zero-copy) | 8 × 1568 B ≈ 12.3 KB | `.Rx_PoolSection` (링커) |
| 0x3000_4000 | lwIP heap (Tx 버퍼) | 14 KB | `LWIP_RAM_HEAP_POINTER` (CubeMX) |

| MPU Region | Base | Size | 속성 | 용도 |
|-----------|------|------|------|------|
| 0 | 0x3000_0000 | 32 KB | Normal, **Non-cacheable** (TEX=1, C=0, B=0) | 버퍼 풀 + heap |
| 1 | 0x3000_0000 | 256 B | **Device** (TEX=0, C=0, B=1) | DMA 디스크립터 |

이 배치는 D2 SRAM 이 32 KB 뿐인 H723/H733 에도 들어가는 크기입니다.
링커 스니펫에는 Rx 풀이 heap(0x3000_4000)을 침범하면 **링크 에러를 내는 ASSERT** 가 들어 있습니다.

---

## 4. STM32CubeMX 설정 순서

> STM32CubeMX 6.4 이상 + STM32CubeH7 FW 1.10 이상 (새 ETH HAL API) 기준입니다.

### 4-1. 프로젝트 생성
- **Board Selector → NUCLEO-H743ZI2** 또는 **MCU Selector → STM32H743ZITx**
- "Initialize all peripherals with their default Mode?" → **No** 를 권장 (필요한 것만 켭니다)

### 4-2. System Core
| 항목 | 설정 |
|------|------|
| RCC | HSE : **BYPASS Clock Source** (Nucleo 는 ST-LINK 의 8 MHz MCO) |
| SYS | Debug : **Serial Wire**, Timebase : SysTick |
| CORTEX_M7 | **CPU ICache : Enabled**, **CPU DCache : Enabled** |
| CORTEX_M7 → MPU Control Mode | **Background Region Privileged accesses only + MPU Disabled during hard fault, NMI and FAULTMASK handlers** |

**MPU Region 0**

| 항목 | 값 |
|------|-----|
| MPU Region | Enabled |
| MPU Region Base Address | **0x30000000** |
| MPU Region Size | **32KB** |
| MPU SubRegion Disable | 0x0 |
| MPU TEX field level | **level 1** |
| MPU Access Permission | ALL ACCESS PERMITTED |
| MPU Instruction Access | ENABLE |
| MPU Shareability Permission | ENABLE |
| MPU Cacheable Permission | **DISABLE** |
| MPU Bufferable Permission | **DISABLE** |

**MPU Region 1**

| 항목 | 값 |
|------|-----|
| MPU Region | Enabled |
| MPU Region Base Address | **0x30000000** |
| MPU Region Size | **256B** |
| MPU TEX field level | **level 0** |
| MPU Access Permission | ALL ACCESS PERMITTED |
| MPU Instruction Access | DISABLE |
| MPU Shareability Permission | ENABLE |
| MPU Cacheable Permission | **DISABLE** |
| MPU Bufferable Permission | **ENABLE** |

### 4-3. Connectivity → ETH
| 항목 | 설정 |
|------|------|
| Mode | **RMII** |
| Parameter Settings → Tx Descriptor Length | **4** |
| First Tx Descriptor Address | 0x30000080 |
| Rx Descriptor Length | **4** |
| First Rx Descriptor Address | 0x30000000 |
| Rx Buffers Length | 1536 |
| GPIO Settings | 핀이 1-1 표와 같은지 확인 → 전부 **Maximum output speed : Very High** |
| NVIC Settings | ETH global interrupt : **사용 안 함** (이 예제는 폴링) |

> 디스크립터 주소와 버퍼 길이는 교체할 ethernetif.c 가 직접 정합니다.
> `Tx/Rx Descriptor Length` 는 `stm32h7xx_hal_conf.h` 의 `ETH_TX/RX_DESC_CNT` 로 들어가므로 **4 로 유지**하세요.

### 4-4. GPIO
| 핀 | 모드 | User Label | 비고 |
|----|------|-----------|------|
| PG0 (예시) | GPIO_Output, 초기값 **High** | **PHY_RST** | LAN8770 nRESET. 없으면 생략 가능 |
| PB0 | GPIO_Output | **LED_GREEN** | Nucleo LD1 : 링크 Up 이면 켜짐 (선택) |

### 4-5. Connectivity → USART3 (로그)
- Mode : **Asynchronous**, 115200-8-N-1, PD8(TX)/PD9(RX) → Nucleo 는 ST-LINK VCP 로 연결됨
- 로그가 필요 없으면 `lan8770_conf.h` 에서 `APP_USE_UART_LOG 0`

### 4-6. Middleware → LWIP
| 탭 | 항목 | 값 |
|----|------|-----|
| General Settings | LWIP_DHCP | **Disabled** |
| | IP_ADDRESS | 192.168.10.10 |
| | NETMASK_ADDRESS | 255.255.255.0 |
| | GATEWAY_ADDRESS | 192.168.10.1 |
| Key Options (Show Advanced Parameters) | MEM_SIZE | **14336** (14 KB) |
| | LWIP_RAM_HEAP_POINTER | **0x30004000** |
| Checksum | CHECKSUM_BY_HARDWARE | Enabled |
| Platform Settings | Driver_PHY | **LAN8742** (자리만 채움 → ethernetif.c 교체로 LAN8770 사용) |

### 4-7. Clock Configuration
- HSE 8 MHz → PLL1 → **SYSCLK 400 MHz**, HCLK 200 MHz (안전한 기본값. Rev V 칩은 480 MHz 도 가능)

### 4-8. Project Manager
- Toolchain/IDE : **STM32CubeIDE** → **GENERATE CODE**

---

## 5. 생성된 프로젝트에 코드 반영

### 5-1. 파일 복사 (스크립트)

```bash
# Linux / macOS / Git-Bash(Windows)
./tools/install_to_cubeide.sh  ~/STM32CubeIDE/workspace/MyH7_LAN8770
```

스크립트가 하는 일:
- `lan8770.c/h` → `Drivers/BSP/Components/lan8770/`
- `ethernetif.c/h` → `LWIP/Target/` (**덮어쓰기**)
- `app_net.c/h`, `lan8770_conf.h` → `Core/` (`lan8770_conf.h` 는 이미 있으면 보존)
- `*_FLASH.ld` 에 `.lwip_sec` 섹션 + ASSERT 추가 (원본은 `.bak` 으로 남김)

> ⚠ **CubeMX 에서 코드를 다시 생성하면 `LWIP/Target/ethernetif.c` 가 LAN8742 버전으로 되돌아갑니다.**
> 재생성 후에는 스크립트를 다시 실행하세요. 링커와 `lan8770_conf.h` 는 그대로 유지됩니다.

수동으로 할 때는 위 목록대로 복사하고, `linker/lwip_sec_snippet.ld` 의 `.lwip_sec` 블록을
`STM32H743ZITX_FLASH.ld` 의 `/DISCARD/` 앞에 붙여 넣으면 됩니다.

### 5-2. Include 경로 추가 (CubeIDE)

**Project → Properties → C/C++ Build → Settings → MCU GCC Compiler → Include paths** 에 추가:

```
../Drivers/BSP/Components/lan8770
```

(`Drivers/BSP/Components/lan8742` 는 남아 있어도 참조하는 곳이 없으므로 무해합니다.)

### 5-3. main.c (USER CODE 영역만 수정 — 재생성해도 유지됨)

```c
/* USER CODE BEGIN Includes */
#include "app_net.h"
/* USER CODE END Includes */
```

```c
  /* USER CODE BEGIN SysInit */
  /* ETH 디스크립터/버퍼/lwIP heap 이 있는 D2 SRAM 클럭 */
#if defined(RCC_AHB2ENR_D2SRAM1EN)
  __HAL_RCC_D2SRAM1_CLK_ENABLE();
#endif
#if defined(RCC_AHB2ENR_D2SRAM2EN)
  __HAL_RCC_D2SRAM2_CLK_ENABLE();
#endif
#if defined(RCC_AHB2ENR_D2SRAM3EN)
  __HAL_RCC_D2SRAM3_CLK_ENABLE();
#endif
  /* USER CODE END SysInit */
```

```c
  /* USER CODE BEGIN 2 */
  APP_Net_Init();          /* MX_LWIP_Init() 다음 */
  /* USER CODE END 2 */

  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    MX_LWIP_Process();     /* 수신 처리 + lwIP 타이머 + 100ms 마다 링크 확인 */
    APP_Net_Process();     /* 링크 변화/상태 로그 */
  }
  /* USER CODE END 3 */
```

> CubeMX 의 NO_SYS 모드는 `MX_LWIP_Process()` 를 while 루프에 **자동으로 넣어주지 않습니다.**
> 빠뜨리면 링크도 안 올라오고 ping 도 안 됩니다.

### 5-4. MPU 를 GUI 대신 코드로 넣고 싶다면

CubeMX 의 MPU 설정(4-2) 대신 `MPU_Config()` 의 USER CODE 에 직접 써도 됩니다.

```c
static void MPU_Config_ETH(void)
{
  MPU_Region_InitTypeDef r = {0};

  HAL_MPU_Disable();

  /* Region 0 : 0x30000000, 32KB, Normal Non-cacheable (Rx 풀 + lwIP heap) */
  r.Enable           = MPU_REGION_ENABLE;
  r.Number           = MPU_REGION_NUMBER0;
  r.BaseAddress      = 0x30000000;
  r.Size             = MPU_REGION_SIZE_32KB;
  r.SubRegionDisable = 0x00;
  r.TypeExtField     = MPU_TEX_LEVEL1;
  r.AccessPermission = MPU_REGION_FULL_ACCESS;
  r.DisableExec      = MPU_INSTRUCTION_ACCESS_ENABLE;
  r.IsShareable      = MPU_ACCESS_SHAREABLE;
  r.IsCacheable      = MPU_ACCESS_NOT_CACHEABLE;
  r.IsBufferable     = MPU_ACCESS_NOT_BUFFERABLE;
  HAL_MPU_ConfigRegion(&r);

  /* Region 1 : 0x30000000, 256B, Device (DMA 디스크립터) */
  r.Number           = MPU_REGION_NUMBER1;
  r.Size             = MPU_REGION_SIZE_256B;
  r.TypeExtField     = MPU_TEX_LEVEL0;
  r.DisableExec      = MPU_INSTRUCTION_ACCESS_DISABLE;
  r.IsBufferable     = MPU_ACCESS_BUFFERABLE;
  HAL_MPU_ConfigRegion(&r);

  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
}
```

### 5-5. 설정 파일 `Core/Inc/lan8770_conf.h`

| 매크로 | 기본값 | 설명 |
|--------|-------|------|
| `APP_PHY_ADDR` | `LAN8770_ADDR_AUTO` | 0~31 자동 탐색. strap 주소를 알면 숫자로 고정 |
| `APP_PHY_ROLE` | `LAN8770_ROLE_MASTER` | **상대편과 반대로**. `LAN8770_ROLE_KEEP` 은 EVB strap 유지 |
| `APP_PHY_RETRY_MS` | 1000 | PHY 를 못 찾았을 때 재시도 주기 |
| `APP_MAC_ADDR0~5` | 02:80:E1:87:70:01 | 로컬 관리 MAC (같은 망에 여러 대면 바꾸세요) |
| `APP_UDP_ECHO_PORT` | 7 | UDP 에코 포트 |
| `APP_STATUS_PERIOD_MS` | 2000 | 상태 로그 주기 |
| `APP_USE_UART_LOG` | 1 | `huart3` 로 printf |

---

## 6. 빌드 & 테스트

1. CubeIDE 에서 **Build (Ctrl+B)** → **Run/Debug**
2. 터미널(115200) 로그 예시

```
=== STM32H7 + LAN8770 (100BASE-T1) ===
[PHY] LAN8770 found: addr=4 rev=1 role=MASTER
[APP] UDP echo server on port 7
[NET] link DOWN  (MASTER) - partner must be the opposite role
[NET] link UP  100BASE-T1 MASTER  SQI=7/7  IP=192.168.10.10  udp_rx=0
```

3. PC 를 같은 서브넷(예: 192.168.10.2/24)으로 설정하고 미디어 컨버터에 연결

```bash
ping 192.168.10.10
python tools/udp_echo_test.py 192.168.10.10 -n 1000 -s 1024
```

```
target 192.168.10.10:7  size=1024B  count=1000
  ok=1000  lost=0  mismatch=0
  RTT min/avg/max = 0.210 / 0.260 / 0.900 ms
```

> RTT 수치는 예시입니다. 실제 값은 PC, 미디어 컨버터, 클럭 설정에 따라 다릅니다.

### 추천 확인 순서
1. **MDIO** : 로그에 `LAN8770 found` 가 나오는지 (전원, MDC/MDIO, 리셋 확인)
2. **링크** : 역할을 상대와 반대로 설정 → `link UP` + EVB 링크 LED
3. **SQI** : 5 이상이면 양호. 낮으면 케이블/커넥터/종단 점검
4. **데이터** : ping → UDP 에코 (RMII 배선 품질, MPU/링커 설정 확인)

---

## 7. 동작 설명

### 7-1. LAN8770 초기화 (`LAN8770_Init`)

```
PHY ID 확인 (Reg2=0x0007, Reg3=0xC15x)  ── 주소 0~31 자동 탐색
   ↓
소프트 리셋 (Reg0 bit15, 완료까지 폴링)
   ↓
HW init 시작 (AFE 0x0B=0x002D, 0x1A=0x0308)
   ↓
MASTER/SLAVE 설정 (Reg9 bit11)
   ↓
SLAVE 이면 Equalizer freeze (DSP 0x69~0x6E)
   ↓
DSP/AFE 초기화 테이블 (CDR, TX 필터, SQI 설정) → PCS 리셋 → HW init 완료 대기 (0x10 bit7)
```

- 확장 레지스터는 `EXT_REG_CTL(0x14)` / `RD_DATA(0x15)` / `WR_DATA(0x16)` 로 **뱅크(MISC/PCS/AFE/DSP)** 를 골라 간접 접근합니다
- 초기화 값은 Microchip 이 LAN87xx 용으로 공개한 값이며, Linux 커널
  `drivers/net/phy/microchip_t1.c` 의 `lan87xx_phy_init()` 에도 같은 값이 있습니다.
  **양산 전에는 사용하는 칩 리비전의 데이터시트와 에라타로 반드시 확인하세요.**

### 7-2. 링크 처리 (`ethernet_link_check_state`, 약 100 ms 주기)

- 링크 판단 : **T1 Mode Status(0x11) bit0** (100BASE-T1 은 BMSR 대신 이 레지스터를 사용)
- 링크 Up → MAC 을 **100 Mbps / Full Duplex 로 고정**하고 `HAL_ETH_Start()` → `netif_set_link_up()`
- 링크 Down → `HAL_ETH_Stop()` → `netif_set_link_down()`
- PHY 초기화에 실패했으면 `APP_PHY_RETRY_MS` 마다 다시 초기화

### 7-3. 동작 중 역할 바꾸기 / 진단 API

```c
LAN8770_SetRole(&LAN8770, LAN8770_ROLE_SLAVE);   /* 재초기화 포함 */

uint8_t sqi;
LAN8770_GetSQI(&LAN8770, &sqi);                 /* 0 ~ 7 */

uint16_t v;
LAN8770_ReadExtReg(&LAN8770, LAN8770_BANK_MISC, 0x08, &v);
```

### 7-4. nINT 인터럽트를 쓰고 싶다면

```c
LAN8770_EnableLinkIT(&LAN8770);     /* 링크 Down + Comm Ready(링크 Up) */

/* EXTI 콜백에서는 플래그만 세우고, main 루프에서: */
uint32_t src;
LAN8770_ClearIT(&LAN8770, &src);    /* 읽으면 클리어 */
ethernet_link_check_state(&gnetif);
```

---

## 8. 문제 해결

| 증상 | 원인 / 확인할 것 |
|------|-----------------|
| 부팅 직후 `Error_Handler()` 에서 멈춤 | **REF_CLK 50 MHz 가 PA1 에 없음** (`HAL_ETH_Init` 실패). 오실로스코프로 확인, EVB strap 확인 |
| `LAN8770 not found on MDIO` | EVB 전원, MDC/MDIO 배선, MDIO 풀업, nRESET 이 High 인지, GND |
| 계속 `link DOWN` | **양쪽 역할이 같음**(MASTER-MASTER), T1 선로(P/N) 배선, 상대가 100BASE-T1 이 아님(1000BASE-T1, 10BASE-T1S 와는 호환 안 됨) |
| 링크 Up 인데 ping 실패 | PC 서브넷, RMII 데이터선(TXD/RXD 뒤바뀜, 너무 긴 점퍼), Nucleo 온보드 PHY 미분리 |
| 수신은 되는데 데이터가 깨짐/가끔 멈춤 | MPU 설정 누락(캐시 일관성), 링커 `.lwip_sec` 누락 → 디스크립터가 DTCM 에 놓이면 DMA 가 접근 못 함 |
| 부팅 시 HardFault | D2 SRAM 클럭(5-3), MPU 영역 크기/주소 |
| 링크 에러 `ETH Rx pool overlaps ...` | `ETH_RX_BUFFER_CNT` 또는 버퍼 크기를 늘렸음 → heap 주소를 뒤로 옮기거나 개수 축소 |
| SQI 가 낮음 (≤3) | 케이블 길이/품질, 커넥터, 공통모드 초크·종단 |
| 재생성 후 LAN8742 로 돌아감 | `tools/install_to_cubeide.sh` 다시 실행 |

---

## 9. 검증 범위

- `arm-none-eabi-gcc 13.2` 로 **STM32CubeH7 HAL(stm32h7xx_hal_driver) + ST lwIP(stm32_mw_lwip)** 와 함께
  STM32H743 대상으로 컴파일과 링크까지 확인했습니다. 디스크립터와 Rx 풀이 0x30000000 / 0x30000080 / 0x30000100 에
  놓이는 것과, 풀이 heap 을 침범할 때 ASSERT 가 링크를 막는 것도 확인했습니다
- **실제 EVB 하드웨어에서는 아직 시험하지 않았습니다.** EVB 커넥터 핀맵, strap 설정(REF_CLK 모드, PHY 주소,
  MASTER/SLAVE), 전원 레일은 EVB 사용자 가이드와 회로도로 확인하세요
