# STM32H7_LAN8770_Ethernet — STM32H743 + Microchip LAN8770 (EV48S68A) 100BASE-T1

**바로 열어서 빌드할 수 있는 독립 STM32CubeIDE 프로젝트**입니다.
HAL, CMSIS, lwIP 소스가 모두 들어 있어서 CubeMX 코드 생성 없이 **Import → Build → Debug** 하면 됩니다.

- 보드 : **NUCLEO-H743ZI2** (STM32H743ZIT6). 다른 STM32H743 보드도 핀만 맞추면 그대로 사용 가능
- PHY : **Microchip EVB-LAN8770-RMII (EV48S68A)**, 100BASE-T1 (오토네고 없음, 100 Mbps Full Duplex 고정)
- 스택 : lwIP 2.1.2 (STM32CubeMX 방식, **NO_SYS = 1 베어메탈**)
- 동작
  - **ping 응답**, **UDP 에코 서버 (포트 7)**
  - USART3(ST-LINK 가상 COM, 115200)로 PHY 주소, MASTER/SLAVE, 링크, **SQI(신호 품질)**, IP 를 2초마다 출력
  - LED : 초록 = 링크 Up, 노랑 = PHY 를 못 찾음, 빨강 = `Error_Handler`
  - PHY 를 못 찾으면 멈추지 않고 1초마다 다시 초기화

```
 NUCLEO-H743ZI2                EVB-LAN8770-RMII (EV48S68A)          상대편 T1 장비
┌────────────────┐   RMII    ┌──────────────────────┐ 100BASE-T1 ┌────────────────┐
│ STM32H743 ETH  │═══════════│ LAN8770              │────────────│ T1 미디어 컨버터 │── PC
│ USART3 → VCP ──┼─ USB(PC)  │ (MASTER or SLAVE)    │  UTP 1pair │ 또는 LAN8770 EVB │
└────────────────┘ MDIO/MDC  └──────────────────────┘            └────────────────┘
```

---

## 1. 빠른 시작

### 1-1. STM32CubeIDE 로 열기
1. 이 폴더(`STM32H7_LAN8770_Ethernet/`)를 PC 에 받습니다
2. STM32CubeIDE → **File → Import → General → Existing Projects into Workspace**
3. **Select root directory** 에 이 폴더 지정 → 프로젝트 `STM32H7_LAN8770_Ethernet` 체크 → Finish
   (작업 공간으로 복사하지 않고 이 폴더를 직접 사용해도 됩니다)
4. **Project → Build Project** (Debug 구성)
5. 보드를 USB 로 연결 → **Run → Debug As → STM32 C/C++ Application**
6. 터미널(115200-8-N-1)로 ST-LINK 가상 COM 포트를 엽니다

### 1-2. 명령줄 빌드 (선택)
```bash
make -j8          # build/STM32H7_LAN8770_Ethernet.elf / .hex / .bin
make flash        # STM32_Programmer_CLI 로 SWD 다운로드
```

### 1-3. 처음 바꿔야 할 설정 — `Core/Inc/lan8770_conf.h`

| 매크로 | 기본값 | 설명 |
|--------|-------|------|
| `APP_PHY_ROLE` | `LAN8770_ROLE_MASTER` | **상대편과 반대로**. `LAN8770_ROLE_KEEP` 은 EVB strap 설정 유지 |
| `APP_PHY_ADDR` | `LAN8770_ADDR_AUTO` | MDIO 주소 0~31 자동 탐색. 알면 숫자로 고정 |
| `APP_MAC_ADDR0~5` | 02:80:E1:87:70:01 | 로컬 관리 MAC |
| `APP_UDP_ECHO_PORT` | 7 | UDP 에코 포트 |
| `APP_STATUS_PERIOD_MS` | 2000 | 상태 로그 주기 |
| `APP_USE_UART_LOG` | 1 | USART3 printf 사용 |

IP 주소는 `LWIP/App/lwip.c` 의 `MX_LWIP_Init()` (기본 **192.168.10.10/24**, GW 192.168.10.1) 또는 CubeMX 의 LWIP 설정에서 바꿉니다.

---

## 2. 하드웨어 연결

### 2-1. 핀 배치 (NUCLEO-H743ZI2)

| 신호 | STM32 핀 | 비고 |
|------|---------|------|
| RMII REF_CLK | **PA1** | 50 MHz **입력** (STM32 는 만들지 않음) |
| RMII MDIO | **PA2** | 1.5 kΩ 풀업 필요 (EVB 에 있는지 확인) |
| RMII CRS_DV | **PA7** | |
| RMII MDC | **PC1** | ≤ 2.5 MHz, HAL 이 자동 분주 |
| RMII RXD0 / RXD1 | **PC4 / PC5** | |
| RMII TX_EN | **PG11** | |
| RMII TXD0 / TXD1 | **PG13 / PB13** | |
| PHY nRESET | **PG0** (`PHY_RST`) | 부팅 시 10 ms Low 펄스 후 High |
| UART 로그 | PD8 (TX) / PD9 (RX) | ST-LINK 가상 COM 으로 연결됨 (배선 불필요) |
| LED | PB0 초록 / PE1 노랑 / PB14 빨강 | 보드 LD1 / LD2 / LD3 |
| GND / 전원 | | GND 는 여러 가닥. EVB 전원 레일은 EVB 사용자 가이드로 확인 |

> EVB 의 RMII 커넥터는 Microchip SAMA5D3 EDS 호스트 규격입니다.
> **커넥터 핀 번호는 EVB 회로도에서 반드시 확인**하세요.

### 2-2. ⚠ 온보드 PHY 분리 (NUCLEO-H743ZI2)

보드에 있는 **LAN8742 PHY 가 위 RMII 핀에 이미 연결**되어 있습니다.
특히 PA1/PA7/PC4/PC5 는 대체 핀이 없습니다. **UM2407(보드 매뉴얼)의 Ethernet 절을 보고 해당 솔더 브리지를 제거**해
온보드 PHY 를 분리하세요. 분리하지 않으면 두 PHY 가 RXD/CRS_DV 를 동시에 구동합니다.
(드라이버는 PHY ID 로 LAN8770 만 골라내므로 MDIO 공유는 괜찮습니다.)

### 2-3. REF_CLK 50 MHz

| 방식 | 설명 |
|------|------|
| **A. PHY 가 출력** | LAN8770 이 25 MHz 크리스탈로 50 MHz 를 만들어 출력 (strap 설정) → PA1 에 연결. EVB 기본값을 회로도로 확인 |
| B. 외부 50 MHz 오실레이터 | PHY REF_CLK 입력과 PA1 에 함께 연결 |
| C. STM32 MCO1(PA8) | 50 MHz 를 만들어 PHY 와 PA1 에 함께 연결 (지터 주의) |

REF_CLK 가 없으면 `HAL_ETH_Init()` 이 실패해 **빨간 LED** 가 켜지고 멈춥니다.

### 2-4. 배선 품질과 상대편
- 50 MHz 신호입니다. 점퍼선은 **10 cm 이하**, 가능하면 어댑터 PCB 를 권장합니다
- 100BASE-T1 은 일반 PC 랜카드에 바로 연결되지 않습니다 → **100BASE-T1 ↔ 100BASE-TX 미디어 컨버터** 또는 다른 T1 장비가 필요합니다
- 링크 양 끝 중 **하나는 MASTER, 하나는 SLAVE**

---

## 3. 프로젝트 구성

```
STM32H7_LAN8770_Ethernet/
├── .project / .cproject                  STM32CubeIDE 프로젝트 (Debug / Release)
├── STM32H7_LAN8770_Ethernet.ioc          STM32CubeMX 설정 (핀, 클럭, MPU, ETH, LWIP, USART3)
├── STM32H743ZITX_FLASH.ld                링커 스크립트 (.lwip_sec + 겹침 검사 ASSERT)
├── Makefile                              명령줄 빌드 (CubeIDE 와 같은 소스/옵션)
├── Core/
│   ├── Inc/  main.h  stm32h7xx_hal_conf.h  stm32h7xx_it.h
│   │         lan8770_conf.h ← 사용자 설정      app_net.h
│   ├── Src/  main.c  stm32h7xx_it.c  stm32h7xx_hal_msp.c  system_stm32h7xx.c
│   │         syscalls.c  sysmem.c   app_net.c ← UDP 에코 + 상태 로그
│   └── Startup/startup_stm32h743zitx.s
├── LWIP/
│   ├── App/     lwip.c  lwip.h                MX_LWIP_Init / MX_LWIP_Process
│   └── Target/  ethernetif.c  ethernetif.h    ← LAN8770 용 (RMII, 100M FD, zero-copy Rx)
│                lwipopts.h                    NO_SYS, heap 0x30004000
├── Drivers/
│   ├── BSP/Components/lan8770/  lan8770.c  lan8770.h   ← PHY 드라이버
│   ├── STM32H7xx_HAL_Driver/   (ST HAL v1.11.5, BSD-3-Clause)
│   └── CMSIS/                  (Arm/ST, Apache-2.0)
├── Middlewares/Third_Party/LwIP/  (lwIP 2.1.2 ST 포팅, BSD-3-Clause)
└── tools/udp_echo_test.py         PC 쪽 UDP 에코 테스트
```

### 실행 흐름

```
main()
 ├─ MPU_Config()          0x30000000: 32KB non-cacheable + 256B device
 ├─ I/D-Cache Enable, HAL_Init(), SystemClock_Config()   HSI → 400 MHz
 ├─ D2 SRAM 클럭 Enable
 ├─ MX_GPIO_Init()        LED, PHY_RST(High)
 ├─ MX_USART3_UART_Init() 115200
 ├─ MX_LWIP_Init()        lwip_init → netif_add → ethernetif_init
 │                         └─ PHY 리셋 → HAL_ETH_Init(RMII) → LAN8770_Init
 ├─ APP_Net_Init()        UDP 에코 bind, 배너 출력
 └─ while(1)
      ├─ MX_LWIP_Process()   수신 → lwIP, 타이머, 100 ms 마다 링크 확인
      └─ APP_Net_Process()   LED, 2초마다 상태 로그
```

---

## 4. 메모리 배치 (STM32H7 의 핵심 주의점)

ETH DMA 는 **DTCM(0x2000_0000)에 접근할 수 없습니다.** 그래서 디스크립터와 버퍼를 D2 SRAM 에 두고
MPU 로 캐시를 끕니다.

| 주소 | 내용 | 크기 |
|------|------|------|
| 0x3000_0000 | Rx DMA 디스크립터 (`.RxDecripSection`) | 4 × 24 B |
| 0x3000_0080 | Tx DMA 디스크립터 (`.TxDecripSection`) | 4 × 24 B |
| 0x3000_0100 | Rx 버퍼 풀 (`.Rx_PoolSection`) | 8 × 1568 B ≈ 12.3 KB |
| 0x3000_4000 | lwIP heap (`LWIP_RAM_HEAP_POINTER`) | 14 KB |

| MPU | 영역 | 속성 |
|-----|------|------|
| Region 0 | 4 GB 배경 | No access (CubeMX 기본) |
| Region 1 | 0x3000_0000, 32 KB | Normal, **Non-cacheable** |
| Region 2 | 0x3000_0000, 256 B | **Device** (디스크립터) |

링커 스크립트에는 Rx 풀이 heap 을 침범하면 링크 에러를 내는 `ASSERT` 가 있습니다.
빌드 결과는 `RAM_D2: 12803 B` 이고 풀은 `0x30003203` 에서 끝납니다.

---

## 5. 테스트

### UART 로그 예시
```
=== STM32H7 + LAN8770 (100BASE-T1) ===
[PHY] LAN8770 found: addr=4 rev=1 role=MASTER
[APP] UDP echo server on port 7
[NET] link DOWN  (MASTER) - partner must be the opposite role
[NET] link UP  100BASE-T1 MASTER  SQI=7/7  IP=192.168.10.10  udp_rx=0
```
(주소, 리비전, SQI 값은 예시입니다.)

### PC 에서
PC 를 같은 서브넷(예: 192.168.10.2/24)으로 설정하고 미디어 컨버터에 연결합니다.
```bash
ping 192.168.10.10
python tools/udp_echo_test.py 192.168.10.10 -n 1000 -s 1024
```

### 확인 순서
1. **MDIO** : `LAN8770 found` 로그 (노란 LED 꺼짐)
2. **링크** : 역할을 상대와 반대로 → `link UP` + 초록 LED
3. **SQI** : 5 이상이면 양호
4. **데이터** : ping → UDP 에코

---

## 6. LAN8770 드라이버 요약 (`Drivers/BSP/Components/lan8770`)

```
LAN8770_Init(obj, addr, role)
  PHY ID 확인 (Reg2=0x0007, Reg3=0xC15x, 주소 자동 탐색)
  → 소프트 리셋 → HW init 시작
  → MASTER/SLAVE 설정 (Reg9 bit11) → SLAVE 면 Equalizer freeze
  → DSP/AFE 초기화 테이블 → PCS 리셋 → HW init 완료 대기
```

| API | 설명 |
|-----|------|
| `LAN8770_GetLinkState` | T1 Mode Status(0x11) bit0 → `LINK_DOWN` / `100MBITS_FULLDUPLEX` |
| `LAN8770_SetRole / GetRole` | MASTER/SLAVE 변경(재초기화 포함) / 읽기 |
| `LAN8770_GetSQI` | 신호 품질 0~7 |
| `LAN8770_EnableLinkIT / ClearIT` | nINT 핀용 링크 인터럽트 |
| `LAN8770_Read/WriteExtReg` | MISC/PCS/AFE/DSP 뱅크 확장 레지스터 |
| `LAN8770_Enable/DisableLoopbackMode` | 루프백 |

초기화 값은 Microchip 이 LAN87xx 용으로 공개한 값입니다(Linux `drivers/net/phy/microchip_t1.c` 에도 같은 값이 있음).
**양산 전에는 사용하는 칩 리비전의 데이터시트와 에라타로 확인하세요.**

---

## 7. STM32CubeMX 로 설정을 바꿀 때

`.ioc` 를 열면 핀, 클럭, MPU, ETH, LWIP, USART3 설정을 GUI 로 볼 수 있고 바꿀 수도 있습니다.
**코드를 다시 생성할 때는 아래를 지키세요.**

- `USER CODE BEGIN/END` 사이의 코드는 유지됩니다 (`main.c` 의 D2 SRAM 클럭, `APP_Net_*` 호출 등)
- ⚠ **`LWIP/Target/ethernetif.c` 는 CubeMX 가 LAN8742 버전으로 덮어씁니다.** 생성 후 바로 되돌리세요
  ```bash
  git checkout -- LWIP/Target/ethernetif.c
  ```
- CubeMX 가 `Drivers/BSP/Components/lan8742` 를 다시 만들 수 있지만 참조하는 곳이 없으므로 무해합니다
- 생성 후에도 `.cproject` 의 include 경로에 `../Drivers/BSP/Components/lan8770` 이 있는지 확인하세요
- 링커 스크립트를 CubeMX 가 새로 만들었다면 `.lwip_sec` 블록을 다시 넣어야 합니다 (git diff 로 확인)

> `.ioc` 는 ST 가 공개한 NUCLEO-H743ZI2 Ethernet 예제(STM32CubeMX 6.14.1)를 바탕으로
> FreeRTOS 를 빼고 이 프로젝트에 맞게 고친 것입니다. CubeMX 버전에 따라 열 때 마이그레이션을 물어볼 수 있습니다.

---

## 8. 문제 해결

| 증상 | 원인 / 확인할 것 |
|------|-----------------|
| 부팅 직후 빨간 LED | **REF_CLK 50 MHz 가 PA1 에 없음** (`HAL_ETH_Init` 실패) |
| 노란 LED + `LAN8770 not found` | EVB 전원, MDC/MDIO 배선, MDIO 풀업, PG0 nRESET 배선, GND |
| 계속 `link DOWN` | 양쪽 역할이 같음, T1 선로(P/N) 배선, 상대가 100BASE-T1 이 아님 (1000BASE-T1/10BASE-T1S 와는 호환 안 됨) |
| 링크 Up 인데 ping 실패 | PC 서브넷, RMII 데이터선(TXD/RXD 뒤바뀜, 너무 긴 점퍼), 온보드 LAN8742 미분리 |
| 데이터가 깨지거나 가끔 멈춤 | MPU/링커 설정이 바뀌었는지 (디스크립터가 0x30000000 에 있는지 `.map` 확인) |
| 링크 에러 `ETH Rx pool overlaps ...` | `ETH_RX_BUFFER_CNT`/버퍼 크기를 늘림 → heap 주소를 옮기거나 개수 축소 |
| SQI 낮음 (≤3) | 케이블 길이/품질, 커넥터, 공통모드 초크·종단 |
| CubeMX 재생성 후 LAN8742 로 바뀜 | `git checkout -- LWIP/Target/ethernetif.c` |

---

## 9. 검증 범위

- `arm-none-eabi-gcc 13.2` 로 이 폴더의 **모든 소스(HAL, lwIP 전체 포함)를 `-Wall` 경고 0개로 빌드·링크**했습니다.
  CubeIDE 와 같은 조건이 되도록 `.cproject` 의 include 경로와 define 을 그대로 써서 한 번 더 빌드했습니다.
  (FLASH 66.6 KB, RAM_D1 21.6 KB, RAM_D2 12.8 KB)
- 디스크립터와 Rx 풀이 0x30000000 / 0x30000080 / 0x30000100 에 놓이는 것을 `.map` 으로 확인했습니다
- **STM32CubeIDE GUI 에서 직접 Import 해보지는 못했고, 실제 EVB 하드웨어에서도 아직 시험하지 않았습니다.**
  `.project`/`.cproject`/`.ioc` 는 ST 가 공개한 CubeMX 생성 프로젝트를 바탕으로 만들었습니다.
- EVB 커넥터 핀맵, strap 설정(REF_CLK 모드, PHY 주소, MASTER/SLAVE), 전원 레일은 EVB 사용자 가이드와 회로도로 확인하세요

## 라이선스

- `Drivers/STM32H7xx_HAL_Driver` : STMicroelectronics, BSD-3-Clause (`LICENSE.txt`)
- `Drivers/CMSIS` : Arm / STMicroelectronics, Apache-2.0 (`LICENSE.txt`)
- `Middlewares/Third_Party/LwIP` : Swedish Institute of Computer Science, BSD-3-Clause (`COPYING`)
