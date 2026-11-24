# STM32H7_QCA7005_PLC — STM32H743 + QCA7005 HomePlug Green PHY (SLAC + DIN 70121)

STM32H743 MCU가 **SPI로 QCA7005 PLC 모뎀을 직접 제어**하고, ISO 15118-3 **SLAC**(충전기와 차량 매칭)를
**충전기(EVSE) 측**과 **차량(EV) 측**으로 나누어 구현한 STM32CubeIDE 프로젝트입니다.
SLAC 다음 단계인 **high level 통신(IPv6 · SDP · V2GTP · EXI · DIN SPEC 70121 DC 충전)** 까지 들어 있습니다.
HAL, CMSIS, lwIP, EXI 코덱이 모두 들어 있어서 **Import → Build → Debug** 만 하면 됩니다.

> **High level 통신 (DIN 70121)**: [`docs/V2G_DIN70121.md`](docs/V2G_DIN70121.md) — 구조, 메시지 순서, 하드웨어 훅, 시험 방법.
> 전력부(충전기 출력)와 배터리는 **시뮬레이션**입니다. 실제 고전압 하드웨어 훅으로 바꾸기 전에는 충전에 쓰면 안 됩니다.

> **STM32CubeMX 설정**: [`docs/CubeMX_설정.md`](docs/CubeMX_설정.md) 에 모든 설정값과 이유를 정리했고,
> 같은 설정의 [`STM32H7_QCA7005_PLC.ioc`](STM32H7_QCA7005_PLC.ioc) 도 들어 있습니다.
> `main.c` / `stm32h7xx_hal_msp.c` / `stm32h7xx_it.c` 는 CubeMX 생성 형식이라 다시 생성해도 사용자 코드가 유지됩니다.

- 보드 : **NUCLEO-H743ZI2** (STM32H743ZIT6). 다른 STM32H7 보드도 핀만 맞추면 그대로 사용할 수 있습니다
- 모뎀 : **Qualcomm QCA7005** (QCA7000 계열 SPI 호스트 인터페이스)
- 역할 : `PLC_ROLE_EVSE` (충전기) / `PLC_ROLE_EV` (차량) — **같은 소스에서 빌드할 때 선택**
- 구조 : 베어메탈 메인 루프, 블로킹 없는 상태 머신, SPI는 폴링 방식(DMA·D-Cache 정합 문제 없음)
- 검증 : PC 시뮬레이션 테스트(가상 QCA SPI 슬레이브, 가상 모뎀 여러 대, 두 프로세스 간 DIN 70121 세션 전체) 통과,
  `arm-none-eabi-gcc`로 두 역할 모두 **경고 없이 빌드** 확인. **실제 보드·모뎀·차량에서는 아직 시험하지 않았습니다**

```
 충전기 (EVSE)                                                     차량 (EV)
┌──────────────────┐ SPI  ┌─────────┐   CP 선 (PLC 신호)  ┌─────────┐ SPI  ┌──────────────────┐
│ STM32H743        │══════│ QCA7005 │~~~~~~~~~~~~~~~~~~~~~│ QCA7005 │══════│ STM32H743        │
│  PLC_ROLE_EVSE   │      │ PIB=EVSE│   + PE (접지)       │ PIB=PEV │      │  PLC_ROLE_EV     │
│  TIM1 CP PWM ────┼──────┼─────────┼──── ±12 V 1 kHz ────┼─────────┼──────┼─ ADC CP 측정     │
│  ADC  CP 측정    │      └─────────┘                     └─────────┘      │  S2 스위치       │
└──────────────────┘                                                        └──────────────────┘
```

---

## 1. STM32H7로 충분한가?

**충분합니다.** QCA7005는 PLC 신호 처리(물리 계층·MAC)를 모두 칩 내부 펌웨어가 하고, 호스트는
**SPI로 이더넷 프레임을 주고받기만** 하면 됩니다. 호스트가 할 일은 SPI 레지스터 프로토콜, 프레임 캡슐화,
SLAC 메시지 처리 정도입니다.

| 항목 | 필요량 | STM32H743 |
|---|---|---|
| SPI | 모드 3, 최대 16 MHz (보통 8 MHz) | SPI1 최대 수십 MHz, 이 프로젝트는 5 MHz |
| 이 프로젝트 Flash / RAM | SLAC 까지 약 42 KB / 15 KB, DIN 70121 포함 약 164 KB / 77 KB | 2 MB / 1 MB |
| 수신 버퍼 | 3,167 B (QCA 읽기 버퍼 전체) | 여유 많음 |
| SLAC 타이밍 | 20~50 ms 간격 전송, 200 ms 응답 대기 | 1 ms 틱으로 충분 |
| TLS (ISO 15118-2 PnC / -20, 다음 단계) | 수백 KB Flash, 수십 KB RAM 추가 | 충분 |

SPI 5 MHz에서 1.5 KB 프레임 하나를 보내는 데 약 2.4 ms가 걸립니다. SLAC 메시지는 60~130 바이트라 1 ms 미만입니다.
H7은 V2G 상위 프로토콜(DIN 70121 / ISO 15118-2)까지 올리기에도 자원이 넉넉합니다.

---

## 2. 빠른 시작

### 2-1. 역할(충전기 / 차량) 선택
`Core/Inc/app_conf.h`
```c
#define PLC_ROLE PLC_ROLE_EVSE      /* 충전기 */
/* #define PLC_ROLE PLC_ROLE_EV */  /* 차량   */
```
명령줄 빌드에서는 `ROLE=EVSE` / `ROLE=EV` 로 덮어씁니다.

### 2-2. STM32CubeIDE
1. **File → Import → General → Existing Projects into Workspace** → 이 폴더 선택 → Finish
2. `app_conf.h` 에서 역할 선택 → **Project → Build Project**
3. **Run → Debug As → STM32 C/C++ Application**
4. ST-LINK 가상 COM 포트를 115200-8-N-1 로 엽니다

### 2-3. 명령줄 빌드
```bash
make ROLE=EVSE -j8
```
```bash
make ROLE=EV -j8
```
```bash
make all-roles -j8
```
결과물: `build/evse/STM32H7_QCA7005_PLC.elf` (충전기), `build/ev/STM32H7_QCA7005_PLC.elf` (차량)

### 2-4. PC 테스트 (하드웨어 없이)
```bash
make test
```

---

## 3. 배선 (NUCLEO-H743ZI2 핀)

| STM32H743 | 기능 | 연결 | 비고 |
|---|---|---|---|
| PA5 | SPI1_SCK | QCA7005 SPI_CLK | 모드 3 (CPOL=1, CPHA=1) |
| PA6 | SPI1_MISO | QCA7005 SPI_MISO | |
| PB5 | SPI1_MOSI | QCA7005 SPI_MOSI | |
| PD14 | GPIO 출력 | QCA7005 SPI_CS_L | 소프트웨어 CS |
| PE13 | EXTI13 (상승 에지) | QCA7005 INTR | 없으면 20 ms 폴링으로 대체 동작 |
| PE14 | GPIO 출력 | QCA7005 RESET_L | 부팅 시 20 ms Low |
| PE9 | TIM1_CH1 | CP 드라이버 입력 | **충전기만**: 1 kHz, High=+12 V, Low=−12 V |
| PA3 | ADC1_INP15 | CP 분압·레벨 시프트 출력 | 양쪽 모두 |
| PE11 | GPIO 출력 | S2 스위치 | **차량만**: 1 = 1.3 kΩ 연결(상태 C) |
| PD8 / PD9 | USART3 | ST-LINK VCP | 로그 115200 |
| PB0 / PE1 / PB14 | LED | 초록 / 노랑 / 빨강 | 4장 참고 |

- 핀은 CubeMX(`.ioc`)에서 **User Label**(`QCA_CS`, `QCA_INT`, `CP_ADC` …)로 정의되고, `app_conf.h` 가 그 라벨 매크로를 씁니다.
  NUCLEO 커넥터 위치는 UM2407을 참고하세요
- **CP 아날로그 회로는 보드마다 다릅니다.** ±12 V 를 ADC 범위(0~3.3 V)로 바꾸는 회로에 맞게
  `CP_ADC_ZERO_MV`(CP 0 V일 때 ADC 전압), `CP_ADC_MV_PER_V`(CP 1 V당 ADC mV)를 맞춰 주세요.
  기본값은 1650 mV / 110 mV (−12~+12 V → 0.33~2.97 V)입니다
- QCA7005의 PLC 결합 회로(CP 선 결합 커패시터·트랜스)는 QCA7005 레퍼런스 설계를 따르세요

---

## 4. 동작

### 4-1. 충전기 (EVSE, `app_evse.c`)
| CP 상태 | 동작 |
|---|---|
| A (+12 V, 미연결) | PWM 끔(+12 V DC), SLAC 정지, 다음 세션용 **새 NMK** |
| B/C/D (차량 연결) | **5 % PWM** 켬 → SLAC 응답 대기 (TT_EVSE_SLAC_init 40 s) |
| SLAC 매칭 | 같은 AVLN 형성 → **SDP 서버 + DIN 70121 SECC** (TCP 61851) |
| DIN 세션 종료 / 실패 | PWM 끔, 커넥터를 뽑을 때까지 새 세션 없음 |
| 40 s 동안 요청 없음 | PWM 4 s 끄고 다시 5 % (재시도) |
| E / F | PWM 끔, SLAC 정지 |

### 4-2. 차량 (EV, `app_ev.c`)
| CP 상태 | 동작 |
|---|---|
| B + 3~7 % PWM (150 ms 확인) | SLAC 시작 (연결당 1회) |
| SLAC 매칭 | 충전기 NMK로 모뎀 키 설정 → 같은 AVLN → **SDP → DIN 70121 세션 전체** |
| SLAC 실패 | 5 % 가 유지되면 5 s 후 재시도 (최대 3회) |
| A / E / F (분리) | V2G 중단, 무작위 개인 NMK로 AVLN 탈퇴, S2·접촉기 열림 |

S2는 DIN 70121 상태 머신이 제어합니다. ChargeParameterDiscovery 뒤 닫고(B → C), SessionStop 이나 실패 시 엽니다.
high level 통신 순서는 [`docs/V2G_DIN70121.md`](docs/V2G_DIN70121.md) 2장을 보세요.

### 4-3. SLAC 메시지 흐름 (`PLC/slac.h`)
```
EV 호스트                          EVSE 호스트              EVSE 모뎀
  |-- CM_SLAC_PARAM.REQ (브로드캐스트) ->|
  |<- CM_SLAC_PARAM.CNF -----------------|
  |-- CM_START_ATTEN_CHAR.IND x3 ------->|
  |-- CM_MNBC_SOUND.IND x10 ------------>|   (모뎀이 감쇠 측정)  |
  |                                      |<- CM_ATTEN_PROFILE.IND (사운드마다)
  |<- CM_ATTEN_CHAR.IND (58그룹 평균) ---|
  |-- CM_ATTEN_CHAR.RSP ---------------->|
  |-- CM_SLAC_MATCH.REQ ---------------->|   (감쇠가 가장 작은 EVSE 선택)
  |<- CM_SLAC_MATCH.CNF (NID, NMK) ------|
  |-- CM_SET_KEY.REQ (자기 모뎀으로) --> 두 모뎀이 같은 AVLN에 가입
```
- 주변 충전기에서 누화(crosstalk)로 여러 EVSE가 응답하면 차량은 **평균 감쇠가 가장 작은 EVSE**를 고릅니다
- 타이밍 상수(ISO 15118-3)는 `PLC/slac.h` 의 `TT_*`, `TP_*`, `C_*` 입니다

### 4-4. LED
| LED | 의미 |
|---|---|
| 초록 켜짐 | SLAC 매칭 + 모뎀이 AVLN 상대국 확인 |
| 초록 깜빡임 | SLAC 매칭, AVLN 확인 대기 |
| 노랑 | 모뎀 동기화됨, SLAC 진행 또는 대기 |
| 빨강 깜빡임 | QCA7005 응답 없음 (SPI 배선·모드·리셋 확인) |
| 빨강 켜짐 | SLAC 실패 / `Error_Handler` |

### 4-5. 로그 예시 (PC 시뮬레이션, `make -C tests v`)
```
[SLAC-EVSE] CM_SET_KEY.CNF result 1, NID EB620AA0154301
[SLAC-EV] -> CM_SLAC_PARAM.REQ RunID 78733AF2DF5FAEB7
[SLAC-EVSE] <- CM_SLAC_PARAM.REQ from 02:00:00:00:01:11
[SLAC-EV] <- CM_SLAC_PARAM.CNF from 02:00:00:00:00:10, 10 sounds
[SLAC-EVSE] <- CM_START_ATTEN_CHAR.IND sounds 10 timeout 600 ms
[SLAC-EVSE] sounding done: 10 sounds, 10 profiles, 58 groups, avg 19 dB
[SLAC-EV] <- CM_ATTEN_CHAR.IND 02:00:00:00:00:10 10 sounds, 58 groups, avg 19 dB
[SLAC-EV] 1 EVSE(s), best 02:00:00:00:00:10 avg 19 dB -> CM_SLAC_MATCH.REQ
[SLAC-EVSE] <- CM_SLAC_MATCH.REQ, -> CNF (NMK handed over)
[SLAC-EV] <- CM_SLAC_MATCH.CNF NID EB620AA0154301 -> CM_SET_KEY
[SLAC-EV] JOIN -> MATCHED
  matched after 1210 ms
```
보드에서는 USART3로 이 로그와 함께 2초마다 상태 한 줄이 나옵니다 (형식 예시).
```
[   8000] QCA ok  rx 31 tx 29 err 0/0 resets 1 | AVLN UP (1) | CP B 8950 mV 5.0 % | SLAC MATCHED
```

---

## 5. QCA7005 쪽에서 꼭 확인할 것

1. **PIB 역할 설정** — 충전기 모뎀은 **EVSE**, 차량 모뎀은 **PEV** 로 설정된 PIB가 플래시에 있어야 합니다.
   EVSE 모뎀만 사운딩을 측정해 `CM_ATTEN_PROFILE.IND` 를 호스트로 올려 줍니다.
   PIB 수정·기록은 PC에서 open-plc-utils(`modpib`, `setpib`, `plctool`)로 하는 것이 편합니다
2. **펌웨어** — QCA7005는 자체 SPI 플래시에서 펌웨어와 PIB로 부팅합니다. 이 프로젝트는 펌웨어를 내려받지 않습니다.
   부팅 후 `VS_SW_VER` 응답으로 펌웨어 문자열을 로그에 출력합니다
3. **SPI 확인** — 시그니처 레지스터(0x1A00)가 `0xAA55` 가 아니면 `[QCA] no signature (read 0x....)` 가 1초마다 나옵니다.
   모드 3, CS, MISO/MOSI 교차, 3.3 V 레벨을 확인하세요
4. **CM_SET_KEY.CNF 결과값** — HomePlug 규격상 성공은 0이지만, QCA 펌웨어는 성공 시 1을 돌려줍니다.
   그래서 값은 로그로만 남기고 실패로 처리하지 않습니다
5. **AVLN 확인** — `VS_NW_INFO`(MMV 0)로 1초마다 상대국 수를 확인합니다.
   이 메시지에 응답하지 않는 펌웨어라면 SLAC 매칭까지만 보고, 링크 확인은 상위 계층(SDP 응답)으로 대신하세요

---

## 6. 소스 구성

```
PLC/                       하드웨어 독립 라이브러리 (HAL 의존 없음, PC 테스트 가능)
  qca7k.c/.h               QCA7000/7005 SPI 드라이버: 레지스터, 동기화·리셋, 인터럽트, 송수신
  qca7k_frame.c/.h         SPI 프레임 캡슐화 (AA AA AA AA | 길이 | 이더넷 | 55 55) 디코더
  hpgp.c/.h                HomePlug GP MME 코드와 필드 오프셋, CM_SET_KEY, VS_SW_VER, VS_NW_INFO, NMK→NID
  slac.h, slac_common.c    공통 정의, ISO 15118-3 타이밍
  slac_evse.c/.h           SLAC 충전기 측 상태 머신
  slac_ev.c/.h             SLAC 차량 측 상태 머신
  plc_modem.c/.h           모뎀 펌웨어 버전, AVLN 상태 조회
  sha256.c/.h              NID 계산용 SHA-256
STM32H7_QCA7005_PLC.ioc    STM32CubeMX 설정 (docs/CubeMX_설정.md 참고)
docs/CubeMX_설정.md        CubeMX 설정값과 이유, 코드 생성 후 할 일
docs/V2G_DIN70121.md       SLAC 이후 high level 통신 (DIN 70121) 설명
V2G/                       IPv6 · SDP · V2GTP · EXI · DIN 70121 (HAL 의존 없음, PC 테스트 가능)
  v2g_netif.c              lwIP netif ↔ QCA7005 드라이버, IPv6 link-local
  v2g_sdp.c                SDP (UDP 15118) 서버 / 클라이언트
  v2gtp.c                  V2GTP 헤더, TCP 스트림 재조립
  v2g_codec.c              EXI 인코드/디코드 (libcbv2g), PhysicalValue
  din_secc.c               DIN 70121 충전기 측 상태 머신
  din_evcc.c               DIN 70121 차량 측 상태 머신
  v2g_hw.h, v2g_sim.c      하드웨어 훅 + 시뮬레이션 (전력부, 배터리)
  lwipopts.h, v2g_conf.h   lwIP 설정, 포트·타임아웃
Middlewares/Third_Party/
  LwIP/                    lwIP 2.1.2 (core, ipv6, ethernet)
  cbv2g/                   libcbv2g EXI 코덱 (common, app_handshake, din)
Core/
  Src/main.c               클럭(400 MHz), SPI1, ADC1, TIM1, RNG, USART3, GPIO 초기화 (CubeMX 생성 형식)
  Src/qca7k_port_stm32.c   드라이버 포팅 계층: HAL SPI, CS, EXTI, RESET, RNG, UID→MAC
  Src/cp.c, Inc/cp.h       Control Pilot: 2 ms 측정(상태 A~F, 듀티), PWM(충전기), S2(차량)
  Src/app_plc.c            드라이버 ↔ 모뎀 조회 ↔ 역할 연결, LED, 상태 로그
  Src/app_evse.c           충전기 측 동작 (PLC_ROLE_EVSE 일 때만 빌드)
  Src/app_ev.c             차량 측 동작 (PLC_ROLE_EV 일 때만 빌드)
  Inc/app_conf.h           역할, 핀 라벨 연결, CP 아날로그 보정값
tests/                     PC 단위 테스트 (gcc + AddressSanitizer)
```

### 다른 MCU로 옮기려면
`PLC/` 는 그대로 쓰고 `qca7k_port_t`(CS, 전이중 전송, 인터럽트 확인, 리셋, ms 틱)와
`slac_io_t.random`(난수) 만 새 MCU에 맞게 구현하면 됩니다.

---

## 7. PC 테스트 내용 (`tests/`)

| 테스트 | 확인 내용 |
|---|---|
| SHA-256 | 표준 테스트 벡터 2개 |
| NMK → NID | HomePlug 기본 NMK(암호 "HomePlugAV") → NID `B0F2E695666B03` |
| 프레임 | 한 버스트 안의 프레임 2개 + 중간 쓰레기 바이트, 잘못된 길이 복구 |
| SPI 드라이버 | 가상 QCA7000 슬레이브: 소프트 리셋 → CPU_ON → 동기화, 버스트 수신, 60 B 패딩·BFR_SIZE, 수신 콜백 안에서 송신, 시그니처 오류 후 재동기화, INT 핀 없는 폴링 |
| SLAC 1:1 | 1.2 초 안에 매칭, 양쪽 모뎀 NMK/NID 일치, AVLN UP, 분리 후 키 변경, 재연결 |
| SLAC 누화 | EVSE 2대(15 dB / 47 dB) → 차량이 15 dB 쪽 선택, 다른 EVSE는 10 s 후 대기로 복귀 |
| 손실 | 잃어버린 CM_ATTEN_CHAR.RSP → EVSE 재전송 후 매칭 |
| 타임아웃 | 충전기 없음 → CM_SLAC_PARAM.REQ 3회 후 실패, EVSE 40 s 타임아웃 |
| V2G 코덱 | PhysicalValue, V2GTP 재조립, SDP 바이트 배치, EXI 인코드 → 디코드 |
| DIN 세션 | 충전기·차량 두 프로세스가 각자 lwIP 로 SDP → TCP → … → CurrentDemand → SessionStop (정상 종료 / 충전기 중단) |

---

## 8. 아직 없는 것 / 다음 단계

- **실제 하드웨어 훅**: 충전기 전력 모듈·절연 감시, 차량 BMS·접촉기 (`V2G/v2g_hw.h`). 지금은 시뮬레이션입니다
- **ISO 15118-2 / -20**: libcbv2g 원본에 코덱이 있으므로 같은 구조로 상태 머신을 추가하면 됩니다
- **TLS / Plug & Charge**: mbedTLS + 인증서. 지금 SDP 는 항상 "TLS 없음" 으로 응답합니다
- **CM_VALIDATE** (선택 사항인 BCB 토글 검증)는 구현하지 않았습니다. 대부분의 차량과 충전기는 사용하지 않습니다
- SPI DMA: 지금은 폴링입니다. 처리량이 더 필요하면 DMA + D-Cache 정리(또는 non-cacheable 영역)로 바꾸세요

---

## 9. 라이선스와 출처

- `PLC/qca7k_frame.c`, `PLC/qca7k.c` : Linux 커널 `drivers/net/ethernet/qualcomm/` (qca_7k_common.c, qca_7k.c, qca_spi.c,
  Copyright Qualcomm Atheros / I2SE GmbH)를 바탕으로 MCU용으로 다시 작성했습니다. 원본은 **GPL-2.0 OR BSD-2-Clause**
  이중 라이선스이고, 여기서는 **BSD-2-Clause** 로 사용합니다
- SLAC 메시지 구조와 코드 값: Qualcomm Atheros **open-plc-utils** (`slac/`, `mme/`, `key/HPAVKeyNID.c`, BSD 계열 라이선스)
- `Middlewares/Third_Party/LwIP` : lwIP 2.1.2, **BSD-3-Clause** (`COPYING`)
- `Middlewares/Third_Party/cbv2g` : chargebyte / EVerest **libcbv2g**, **Apache-2.0** (`LICENSE`, 버전은 `VERSION.txt`)
- 그 밖의 `PLC/`, `V2G/`, `Core/` 코드: BSD-2-Clause
- `Drivers/` : STMicroelectronics STM32H7 HAL v1.11.5 / CMSIS (각 폴더의 LICENSE 참고)
