# STM32L562C + FreeRTOS — WiFi 계측 브릿지

STM32L562C(Cortex-M33) 위에서 FreeRTOS(CMSIS-RTOS2)를 사용해, PC ↔ MCU ↔ ESP32(WiFi) ↔ PC서버,
그리고 FPGA → MCU(트리거+ADC값) 경로를 처리하는 펌웨어 애플리케이션 코드입니다.

이 리포지토리는 **STM32CubeMX가 생성하는 프로젝트에 얹는 애플리케이션 레이어**입니다.
HAL/CMSIS/USB 미들웨어/FreeRTOS 커널 소스 등 벤더 생성 코드는 포함하지 않습니다(그건 CubeMX가 직접
만들어야 정확합니다). 대신 `docs/CubeMX_설정가이드.md`대로 .ioc를 구성해 코드를 생성한 뒤,
`Core/Inc`, `Core/Src` 파일들을 CubeMX가 만든 프로젝트의 동일 폴더에 복사하고,
`freertos.c`, `stm32l5xx_it.c`, `usbd_cdc_if.c`의 `USER CODE` 영역에 안내된 훅을 연결하면 됩니다.

## 요구 기능 요약

1. PC(USART3 + USB CDC, 같은 커맨드/데이터를 미러링)에서 시리얼로 접속해 다음을 설정
   - ESP32가 접속할 AP의 SSID/Password
   - 목적지 서버 IP, Port(기본 50001)
   - DHCP 모드 on/off
   - DHCP off일 때 ESP32 모듈의 정적 IP / Gateway / Netmask
2. 설정값은 SPI2로 연결된 W25Q40CLSNIG(4Mbit NOR Flash)에 저장하고, 부팅 시 읽어와 WiFi 접속에 사용
3. PC서버에 연결되면, FPGA(자체적으로 ADC를 읽음)가 일정 간격으로
   - GPIO falling-edge 트리거 신호를 먼저 보내고
   - 이어서 ADC 측정값을 UART(USART2)로 전송
4. MCU는 수신한 측정값을
   - ESP32(USART1, AT 커맨드)를 통해 PC서버로 전송
   - 동시에 USART3 및 USB(CDC)로 PC에도 동일하게 전송

## 설계상 가정 (요청 원문에 명시되지 않아 합리적 기본값으로 채택)

- **ESP32 연동 방식**: Espressif 표준 **ESP-AT** 펌웨어 사용 (MCU가 AT 커맨드로 제어). ESP32 측
  펌웨어 개발 불필요, ESP32에 [espressif/esp-at](https://github.com/espressif/esp-at) 펌웨어를
  플래싱한다고 가정.
- **FPGA→MCU ADC 데이터 채널**: FPGA가 자체적으로 ADC를 읽어, GPIO EXTI(falling edge, 예: PB0)로
  트리거 신호를 먼저 보낸 뒤, 측정값을 **USART2**(FPGA→MCU 전용 UART, ASCII 라인 프로토콜)로
  전송합니다 (SPI2는 플래시 전용). MCU는 트리거 인터럽트 후 USART2로 도착하는 한 줄(`ADC ...`)을
  타임아웃 내에 수신해 파싱합니다.
- **PC ↔ MCU 설정 프로토콜**: 사람이 읽기 쉬운 ASCII 라인 커맨드(`\r\n` 종료). 상세는
  `docs/프로토콜_명세.md` 참고.
- ADC 측정값 포맷: FPGA가 트리거 후 `ADC <seq> <sample0> [sample1 ...]\r\n` 형식의 ASCII 라인을
  USART2로 보낸다고 가정. 정확한 포맷은 `docs/프로토콜_명세.md` §3에서 조정 가능.

위 가정과 다르게 구현하고 싶은 부분이 있으면 알려주시면 반영하겠습니다.

## 폴더 구조

```
docs/
  CubeMX_설정가이드.md   - .ioc 설정 항목 (클럭/핀맵/USART/SPI/USB/FreeRTOS 태스크,우선순위,메모리)
  프로토콜_명세.md        - PC 커맨드셋, 플래시 메모리 맵, ESP32 AT 시퀀스, FPGA 프레임 포맷
Core/Inc/
  app_config.h         - 태스크 우선순위/스택/큐 크기, 프로토콜 상수, 핀 매핑 정의
  ring_buffer.h         - SPSC 바이트 링버퍼
  uart_line_rx.h        - idle-line + Circular DMA UART 수신 공용 헬퍼
  measurement_msg.h     - FPGA 측정값 메시지 구조체 + DATA 라인 빌더
  w25q40.h              - SPI2 NOR 플래시(W25Q40CLSNIG) 드라이버
  net_config_store.h    - 설정 구조체 + CRC32 + 플래시 로드/세이브
  esp32_at.h            - ESP32(USART1) AT 커맨드 드라이버
  pc_comm.h             - PC 커맨드 파서 (USART3 + USB CDC 공용)
  fpga_link.h           - FPGA 트리거(EXTI)/ADC(USART2) 수신
  app_freertos.h        - 태스크/큐 생성 진입점 (freertos.c에서 호출)
Core/Src/
  (위 헤더들의 구현)
  app_freertos.c        - App_FreeRTOS_Init(), ESP32_Task/Config_Task 본체
  app_it_callbacks.c    - HAL_UARTEx_RxEventCallback/HAL_GPIO_EXTI_Callback 단일 정의 + dispatch
```

## 빌드 방법 (요약)

1. STM32CubeMX에서 `docs/CubeMX_설정가이드.md`대로 새 프로젝트 생성 (MCU: STM32L562CETx),
   Toolchain/IDE: STM32CubeIDE 선택 후 Generate Code.
2. 생성된 프로젝트의 `Core/Inc`, `Core/Src`에 이 리포지토리의 동일 파일들을 복사(병합).
3. `Core/Src/freertos.c`의 `USER CODE BEGIN Application` 영역에서 `App_FreeRTOS_Init()` 호출
   (자세한 위치는 `app_freertos.c` 상단 주석 참고).
4. `Core/Src/stm32l5xx_it.c`, `USB_DEVICE/App/usbd_cdc_if.c`의 콜백 연결은
   `docs/CubeMX_설정가이드.md`의 "콜백 연결" 절 참고.
5. STM32CubeIDE에서 빌드 후 ST-LINK로 플래싱.

## 테스트 방법

- PC에서 터미널(예: TeraTerm, PuTTY, `screen`)로 USART3 또는 USB CDC 포트에 115200 8N1로 접속.
- `HELP\r\n` 입력 시 지원 커맨드 목록 출력 (구현부는 `pc_comm.c` 참고).
- 설정 후 `SAVE\r\n`으로 플래시에 저장, MCU가 자동으로 WiFi 재접속을 시도.
