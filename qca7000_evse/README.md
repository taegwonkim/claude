# QCA7000 + STM32H7 + FreeRTOS — EVSE SLAC 골격 코드

## 파일 구성

| 파일 | 내용 |
|---|---|
| `src/qca7000.h/.c` | QCA7000 SPI 드라이버 (Linux `qca_spi.c` 로직 포팅). 버스트/레거시 모드, 동기화·리셋 상태머신, DMA |
| `src/hpgp_mme.h` | HomePlug GP / SLAC MME 구조체 (open-plc-utils `slac.h` 기준) |
| `src/slac_evse.h/.c` | ISO 15118-3 SLAC EVSE 상태머신 (FreeRTOS 태스크) |
| `src/qca7000_netif.c` | lwIP netif 연결 (IPv6 link-local → SDP/V2G용) |
| `src/app_example.c` | 통합 예제: MPU, SPI 설정, HAL 콜백, RX 분배, VS_SW_VER 확인, CP 연동 |

## 태스크 구조

```
EXTI(INT 상승엣지) ──notify──▶ [qca7000 task] ──rx_cb──┬─ 0x88E1 → slac_evse_on_frame() ─▶ [slac task]
SPI DMA 완료 ISR ──semaphore──▶      ▲                   └─ 0x86DD → lwIP (tcpip thread)
                                     └── qca7000_send() / tx_alloc+commit (모든 태스크에서 호출 가능)
```

## 통합 순서

1. **CubeMX 설정**
   - SPI: Master, Mode 3 (CPOL High, CPHA 2Edge), 8bit, MSB, NSS Soft, 처음엔 SCK ≤ 8 MHz
   - SPI TX/RX DMA 활성화
   - GPIO: CS(출력, 초기 High), INT(EXTI 상승엣지), RESET(출력, 액티브 Low)
   - RNG 활성화
   - **NVIC 우선순위**: EXTI·SPI·DMA를 `configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY` 이상의 숫자로 설정 (예: 6)
2. **링커 스크립트**: DMA 버퍼 섹션을 D2 SRAM에 배치
   ```
   .dma_buffer (NOLOAD) : { . = ALIGN(32); *(.dma_buffer) } >RAM_D2
   ```
3. **MPU**: `app_mpu_config()`로 해당 영역을 non-cacheable로 설정. 주소와 크기는 칩에 맞게 조정하세요.
4. `main()`에서 HAL을 초기화한 뒤 `app_plc_init()`을 호출하고, 이어서 `vTaskStartScheduler()`를 호출합니다.
5. CP 상태머신에 연결합니다.
   - State B + 5% PWM이 되면 `app_cp_hlc_started()`
   - State A가 되면 `app_cp_unplugged()`

## 브링업 체크리스트

1. 로그에 `QCA7000 state 2`(READY)가 찍히는지 확인합니다. CPU_ON 인터럽트와 시그니처 0xAA55를 확인한 상태입니다.
2. `QCA7000 ... fw=...` 출력을 확인합니다. VS_SW_VER 응답으로, `plctool -r`과 같은 동작입니다.
3. 실제 EV 또는 EV 시뮬레이터를 연결해 `SLAC: matched` 출력을 확인합니다.
4. 통계 로그에서 `rx_errors`, `spi_errors`, `buf_errors`가 0으로 유지되는지 확인합니다.

## 호스트 테스트 결과 (PC에서 HAL/FreeRTOS 스텁으로 실행)

- 모든 소스가 `-Wall -Wextra` 경고 없이 컴파일됩니다. lwIP glue는 실제 lwIP 헤더로 IPv4 on/off 두 설정 모두 확인했습니다.
- SLAC: SET_KEY → PARM → START_ATTEN → 9개 사운드 + 타임아웃 → ATTEN_CHAR(재전송 포함) → MATCH 전 과정을 통과했습니다.
  - NMK "HomePlugAV" → NID `B0F2E695666B03` 테스트 벡터와 일치합니다.
- 드라이버: 가상 QCA7000 SPI 슬레이브로 다음 항목을 확인했습니다.
  - 프레임 인캡슐레이션과 60바이트 패딩
  - WRBUF 공간 부족 시 재시도
  - 한 번 읽기로 여러 프레임 수신
  - EOF 오류 처리
  - WRBUF_ERR → 소프트 리셋 → 복구
  - 시그니처 오류와 리셋 타임아웃

**실기 검증은 하지 않았습니다.** 실제 보드에서는 SPI 타이밍, PIB 설정, 실제 EV와의 상호운용성을 별도로 확인해야 합니다.

## 꼭 확인할 것

- **타이밍 상수**: `slac_evse_default_cfg()` 값을 인증 대상 ISO 15118-3 판본의 표와 대조하세요.
- **CM_SET_KEY 목적지**: VS_SW_VER.CNF에서 얻은 모뎀 MAC을 씁니다. 응답이 없으면 기본값 `00:B0:52:00:00:01`을 씁니다. 사용하는 펌웨어에서 SET_KEY.CNF가 오는지 확인하세요.
- **SET_KEY.CNF RESULT 값**: 펌웨어마다 의미가 달라서 로그만 남기고 거부하지 않습니다.
- **호스트 MAC**: QCA7000 MAC과 다른, 고유한 주소를 써야 합니다.
- **PIB**: EVSE용 PIB인지, SPI 호스트 인터페이스 설정이 맞는지 확인하세요.
- **버스트/레거시 모드**: 기본은 버스트 모드(Linux 기본값과 같음)입니다. 통신이 안 되면 `-DQCA7000_LEGACY_MODE=1`로 비교해 보세요.

## 호스트 테스트 실행

```bash
cd test && make
```
gcc와 OpenSSL(libcrypto)이 필요합니다. HAL/FreeRTOS는 `test/stubs/`의 스텁으로 대체됩니다.
