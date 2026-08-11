# Stm32WifiConfigTool (PC용 C# 도구)

STM32L562C + FreeRTOS 펌웨어(리포지토리의 `firmware/Core/`, `docs/프로토콜_명세.md`)와 시리얼로
통신하는 Windows 데스크톱 도구입니다. **Visual Studio 2022**, WinForms, .NET Framework 4.8 기준으로
작성되어 있으며 ".NET desktop development" 워크로드만 설치되어 있으면 별도 SDK 설치 없이 바로
열고 빌드할 수 있습니다.

## 여는 방법

1. Visual Studio 2022에서 `Stm32WifiConfigTool.sln` 열기
2. F5(디버그 실행) 또는 Ctrl+Shift+B(빌드)

## 구성 (창 1개, 패널 4개 동시 표시)

MCU는 **USB(CDC 가상 COM)** 와 **UART(USART3, 보통 USB-시리얼 변환기 경유)** 두 채널에 항상
동일한 데이터를 미러링합니다. 두 채널 모두 `STX(0x02) + Data1,Data2,...,DataN + CR(0x0D) + LF(0x0A)`
프레임 포맷을 사용합니다(필드는 콤마로 구분, `docs/프로토콜_명세.md` §1). 이 도구는 두 채널을
완전히 독립적으로 연결·해제할 수 있고, 별도 팝업 창을 띄우지 않고 **메인 창 하나 안에서 네 패널을
항상 동시에** 볼 수 있게 도킹 배치했습니다: 좌상단 포트 설정, 중앙상단 WiFi 설정, 우상단 ESP32 상태,
하단 전체 폭 측정값 보기.

MCU가 보내는 비동기 메시지는 **측정값 프레임과 ESP32 상태 프레임을 구분해서 각각 다른 패널에
표시**합니다(둘 다 첫 필드로 구분되며, 응답 대기 로직도 이 둘을 명령 응답과 혼동하지 않도록
`Stm32Protocol.cs`의 화이트리스트 방식으로 분류합니다).

1. **포트 설정** (좌상단, `Panels/PortSettingsPanel.cs`)
   USB/UART 각각 COM 포트, Baud Rate, 읽기/쓰기 타임아웃(ms)을 설정하고 연결/해제합니다.
   다른 패널에서 명령을 보내거나 데이터를 받으려면 먼저 여기서 연결해야 합니다.

2. **WiFi 설정** (중앙상단, `Panels/WifiConfigPanel.cs`)
   SSID/비밀번호, 서버 IP·Port, DHCP on/off, DHCP off일 때의 정적 IP/Gateway/Netmask를 설정합니다.
   - "현재값 읽기": `GET,CONFIG` 프레임을 보내 MCU에 저장된 값을 화면에 채웁니다(비밀번호는 MCU가
     마스킹해서 돌려주므로 표시되지 않습니다 — 바꾸려면 "비밀번호 변경" 체크 후 새로 입력).
   - "저장(SET+SAVE)": 변경된 값을 `SET,...` 프레임들로 순차 전송 후 `SAVE`까지 수행합니다.
     저장되면 MCU가 자동으로 WiFi/서버 재접속을 시도합니다.
   - "상태 조회": `STATUS` 프레임을 보내 ESP32 상태 번호를 즉시 확인합니다(0=DOWN/1=WIFI_UP/2=TCP_UP).
   - 상단 "명령 전송 채널"에서 USB/UART 중 커맨드를 보낼 채널을 고릅니다.

3. **ESP32 상태 보기** (우상단, `Panels/EspStatusPanel.cs`, 신규)
   MCU가 2초 간격으로 자동 브로드캐스트하는 `STATUS,<번호>` 프레임을 표시합니다. 측정값 프레임
   전송 사이사이에 도착하며, 측정값 패널과는 별도로 여기서만 다룹니다.
   - 현재 상태를 큰 글씨로(색상: DOWN=빨강, WIFI_UP=주황, TCP_UP=초록) 표시.
   - "표시 채널": USB / UART / 둘 다.
   - 하단에 수신 이력(시각/채널/번호/텍스트)을 로그로 쌓고, "지우기"로 초기화합니다.

4. **측정값 보기** (하단, 전체 폭, `Panels/MeasurementPanel.cs`)
   FPGA 트리거 결과로 MCU가 보내는 `<DC IP>,<MAC>,data1,...,data6` 프레임("DATA" 같은 태그
   없음, `docs/프로토콜_명세.md` §2)을 표로 보여줍니다. `DC IP`/`MAC`은 이 장치(ESP32)의
   station IP/MAC 주소로, 측정값을 보낸 장치를 구분하는 용도입니다.
   - "표시 채널": USB / UART / 둘 다 — 어느 채널에서 온 데이터를 그릴지 선택.
   - "지우기": 그리드와 이벤트 로그를 모두 비웁니다.
   - "CSV로 저장": 현재까지 쌓인 측정값을 CSV 파일로 내보냅니다.
   - 하단 "이벤트 로그"에는 `EVENT,WIFI_DISCONNECTED` / `EVENT,WIFI_CONNECTED` /
     `EVENT,TCP_CONNECTED` / `EVENT,TCP_CLOSED` 등 MCU의 비동기 알림이 표시됩니다
     (ESP32 상태 번호 자체는 3번 패널에서 별도로 봅니다).

## Visual Studio 디자이너로 폼/패널 편집하기

4개 패널(`PortSettingsPanel`, `WifiConfigPanel`, `EspStatusPanel`, `MeasurementPanel`)과
`SerialChannelPanel`(USB/UART 공용 하위 컨트롤), `MainForm`은 모두 **표준 WinForms 디자이너
구조**(`<이름>.cs` + `<이름>.Designer.cs`)로 되어 있어 Visual Studio에서 더블클릭하면 디자이너
화면이 뜨고 드래그 앤 드롭/속성 창으로 편집할 수 있습니다.

- **`<이름>.Designer.cs`**: `InitializeComponent()`와 컨트롤 필드 선언만 있습니다. 디자이너가
  자동으로 다시 쓰는 영역이므로 보통 직접 편집하지 않고 디자이너 화면에서 조작합니다.
- **`<이름>.cs`**: 실제 동작(이벤트 핸들러, 비즈니스 로직)이 있습니다. 여기서
  `public Xxx() { InitializeComponent(); }`(매개변수 없는 생성자, 디자이너 전용)와
  `public void Initialize(ConnectionManager conn, AppSettings settings) { ... }`
  (런타임 의존성 연결용, `MainForm`이 생성 직후 1회 호출)를 볼 수 있습니다.

**왜 `Initialize()`가 따로 있나요?** WinForms 디자이너는 컨트롤을 화면에 그리기 위해 항상
매개변수 없는 생성자로 인스턴스를 만듭니다. 그런데 이 패널들은 실제로는 `ConnectionManager`,
`AppSettings` 같은 런타임 의존성이 필요합니다. 그래서 생성자는 `InitializeComponent()`만
호출하고, 실제 의존성 연결/이벤트 구독은 `Initialize()`에서 따로 합니다 — 디자이너와
런타임 동작을 분리하는 WinForms의 표준 패턴입니다. 새 패널을 추가할 때도 이 패턴을 따르세요.

**편집 방법**:
1. 솔루션 탐색기에서 `Panels/WifiConfigPanel.cs`(또는 원하는 패널) 더블클릭 → 디자이너 화면.
2. 툴박스에서 컨트롤을 드래그하거나 기존 컨트롤을 선택해 속성 창에서 수정.
3. 이벤트(예: 버튼 Click)는 속성 창의 번개 아이콘 탭에서 더블클릭하면 `<이름>.cs`에 핸들러가
   자동 생성/연결됩니다.
4. 컨트롤에 런타임 값(연결 상태, 설정값 등)을 반영해야 하면 `Initialize()` 메서드를 직접 수정하세요
   (디자이너가 건드리지 않는 영역).

## 설정값 저장 (포트/보레이트/타임아웃 등)

포트 설정(각 채널의 COM 포트/Baud Rate/읽기·쓰기 타임아웃), WiFi 설정 패널의 명령 전송
채널·커맨드 타임아웃, 측정값 보기 패널의 표시 채널·자동 스크롤 여부, ESP32 상태 보기 패널의
표시 채널은 프로그램을 닫을 때 자동으로 다음 위치에 저장되고, 다음 실행 시 그대로 복원됩니다:

```
%AppData%\Stm32WifiConfigTool\settings.ini
```

`Services/AppSettingsStore.cs`가 담당하며, 외부 패키지 없이 단순 `key=value` 텍스트 형식이라
필요하면 직접 열어 수정해도 됩니다. 파일이 없거나 손상된 경우 기본값으로 안전하게 대체합니다.

**SSID/비밀번호/서버 IP 등 WiFi 접속 정보는 이 파일에 저장하지 않습니다** — 그 값들의 원본은
MCU의 W25Q40 플래시이므로, WiFi 설정 패널의 "현재값 읽기"(`GET CONFIG`)로 항상 MCU에서
다시 조회합니다. 특히 비밀번호를 PC에 평문으로 남기지 않기 위한 의도적인 설계입니다.

## 프로젝트 구조

```
Stm32WifiConfigTool/
  Stm32WifiConfigTool.csproj
  Program.cs               - 진입점
  MainForm.cs / .Designer.cs  - 단일 메인 창, 4개 패널을 도킹 배치 + ConnectionManager/설정 소유
  Panels/                   - UserControl(디자이너 지원), MainForm에 모두 도킹되어 표시
    SerialChannelPanel.cs / .Designer.cs  - USB 또는 UART 채널 1개의 연결 UI (PortSettingsPanel이 2개 사용)
    PortSettingsPanel.cs / .Designer.cs
    WifiConfigPanel.cs / .Designer.cs
    EspStatusPanel.cs / .Designer.cs      - ESP32 상태(STATUS,<번호>) 전용 패널
    MeasurementPanel.cs / .Designer.cs
  Services/
    LinkChannel.cs          - Usb/Uart 채널 구분
    SerialLinkService.cs    - 시리얼 연결 1개(연결/해제, 라인 단위 수신, 타임아웃)
    ConnectionManager.cs    - Usb/Uart SerialLinkService 2개를 앱 전체에서 공유
    Stm32Protocol.cs        - STX+CSV+CRLF 프레임 빌더/파서, 메시지 종류 분류(화이트리스트)
    Stm32Commands.cs        - SET/SAVE/GET CONFIG/STATUS async 요청-응답 헬퍼
    AppSettingsStore.cs     - PC측 UI 설정 로드/저장 (%AppData%\Stm32WifiConfigTool\settings.ini)
  Models/
    NetConfig.cs
    MeasurementRecord.cs    - DC IP/MAC + data1..6
    AppSettings.cs          - 저장 대상 설정 모델 (ChannelSettings 등)
```

## 프로토콜 참고

명령어 세트, 응답 포맷, 비동기 EVENT/STATUS/측정값 프레임 포맷은 리포지토리 루트의
`docs/프로토콜_명세.md`를 그대로 따릅니다. MCU 쪽 커맨드 파서는 `firmware/Core/Src/pc_comm.c`,
측정값 송신은 `firmware/Core/Src/fpga_link.c`, ESP32 상태 브로드캐스트/IP·MAC 조회는
`firmware/Core/Src/app_freertos.c`/`firmware/Core/Src/esp32_at.c` 참고.

## 알려진 제한사항

- 한 번에 하나의 커맨드-응답만 진행한다고 가정합니다(버튼 클릭이 겹치지 않는 일반적인 사용 기준).
  같은 채널에 대해 WiFi 설정 패널에서 커맨드 전송 중일 때 동시에 또 다른 커맨드를 보내면
  응답이 뒤섞일 수 있습니다.
- `SerialLinkService`의 읽기 루프는 설정된 읽기 타임아웃으로 폴링합니다. 타임아웃을 너무 짧게
  주면 CPU 사용량이 늘고, 너무 길게 주면 연결 해제 감지가 느려질 수 있습니다(기본값 3000ms 권장).
