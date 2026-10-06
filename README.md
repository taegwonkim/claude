# STM32L562C WiFi 계측 브릿지

STM32L562C(MCU) 펌웨어와 이를 설정/모니터링하는 Windows PC용 C# 도구로 구성된 저장소입니다.
펌웨어는 **FreeRTOS 버전과 RTOS 미사용(super-loop) 버전 두 가지 구현**을 제공하며, 어느 쪽을
쓰든 PC 도구와의 통신 규격은 동일합니다(둘 중 하나만 골라 CubeIDE 프로젝트에 넣으면 됩니다).
각 프로젝트는 별도로 빌드/배포되며, 시리얼 링크(USART3 + USB CDC)로 통신할 때만 서로를 알면
됩니다 — 그 통신 규격이 아래 공유 문서입니다.

## 구성

```
firmware/           - STM32L562C + FreeRTOS 펌웨어 (STM32CubeIDE, C). firmware/README.md 참고.
firmware-no-rtos/    - 위와 동일 기능/프로토콜, RTOS 없이 super-loop로 구현한 버전.
                      firmware-no-rtos/README.md 참고 (RTOS 미사용 시의 실시간성 트레이드오프 설명).
pc-app/              - PC용 C# 설정/모니터링 도구 (Visual Studio 2022, WinForms, .NET Framework 4.8).
                      자세한 내용은 pc-app/README.md 참고. 위 두 펌웨어 중 어느 쪽과 연결해도 동작 동일.
docs/
  프로토콜_명세.md  - firmware(-no-rtos) ↔ pc-app 공용 통신 규격(PC↔MCU STX+CSV+CRLF 프레임,
                    FPGA 프레임 포맷, ESP32 AT 시퀀스, 플래시 메모리 맵). 한쪽만 수정하면 반드시
                    문서와 상대편 구현(가능하면 양쪽 펌웨어 모두)을 함께 확인하세요.
```

## 시작하기

- **펌웨어 개발/빌드 (FreeRTOS)**: [`firmware/README.md`](firmware/README.md)
  (STM32CubeMX 설정은 [`firmware/docs/CubeMX_설정가이드.md`](firmware/docs/CubeMX_설정가이드.md))
- **펌웨어 개발/빌드 (RTOS 미사용)**: [`firmware-no-rtos/README.md`](firmware-no-rtos/README.md)
- **PC 도구 개발/빌드**: [`pc-app/README.md`](pc-app/README.md)
- **펌웨어 ↔ PC 도구 통신 프로토콜**: [`docs/프로토콜_명세.md`](docs/프로토콜_명세.md)

## 전체 시스템 개요

1. PC(USART3 + USB CDC, 같은 커맨드/데이터를 미러링)에서 시리얼로 MCU에 접속해 ESP32가 접속할
   AP의 SSID/Password, 목적지 서버(PC) IP/Port, DHCP 모드 등을 설정합니다.
2. 설정값은 MCU가 SPI2로 연결된 W25Q40CLSNIG 플래시에 저장하고, 부팅 시 읽어와 WiFi 접속에
   사용합니다.
3. MCU가 부팅 후 FPGA에 측정 개시 커맨드를 1회 보내면, FPGA(자체적으로 ADC를 읽음)가 일정
   간격으로 GPIO 트리거 신호 → ADC 측정값(UART)을 MCU에 전송합니다.
4. MCU는 수신한 측정값을 ESP32(WiFi)를 통해 PC서버로 전송하는 동시에, USART3/USB로 PC 도구에도
   동일하게 미러링합니다.

각 항목의 상세 설계/구현은 `firmware/README.md`와 `docs/프로토콜_명세.md`를 참고하세요.
