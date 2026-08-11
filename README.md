# STM32L562C WiFi 계측 브릿지

STM32L562C(MCU) 펌웨어와 이를 설정/모니터링하는 Windows PC용 C# 도구, 이렇게 **서로 독립된
두 프로젝트**로 구성된 저장소입니다. 둘은 별도로 빌드/배포되며, 시리얼 링크(USART3 + USB CDC)로
통신할 때만 서로를 알면 됩니다 — 그 통신 규격이 아래 공유 문서입니다.

## 구성

```
firmware/   - STM32L562C + FreeRTOS 펌웨어 (STM32CubeIDE, C). 자세한 내용은 firmware/README.md 참고.
pc-app/     - PC용 C# 설정/모니터링 도구 (Visual Studio 2022, WinForms, .NET Framework 4.8).
              자세한 내용은 pc-app/README.md 참고.
docs/
  프로토콜_명세.md  - firmware ↔ pc-app 공용 통신 규격(PC↔MCU STX+CSV+CRLF 프레임, FPGA 프레임
                    포맷, ESP32 AT 시퀀스, 플래시 메모리 맵). 한쪽만 수정하면 반드시 문서와
                    상대편 구현을 함께 확인하세요.
```

## 시작하기

- **펌웨어 개발/빌드**: [`firmware/README.md`](firmware/README.md)
  (STM32CubeMX 설정은 [`firmware/docs/CubeMX_설정가이드.md`](firmware/docs/CubeMX_설정가이드.md))
- **PC 도구 개발/빌드**: [`pc-app/README.md`](pc-app/README.md)
- **두 프로젝트 사이의 통신 프로토콜**: [`docs/프로토콜_명세.md`](docs/프로토콜_명세.md)

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
