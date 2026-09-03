# Stm32WifiConfigTool (PC용 C# 도구)

STM32L562C + FreeRTOS 펌웨어(리포지토리의 `firmware/Core/`, `docs/프로토콜_명세.md`)와 시리얼로
통신하는 Windows 데스크톱 도구입니다. **Visual Studio 2022**, WinForms, .NET Framework 4.8 기준으로
작성되어 있으며 ".NET desktop development" 워크로드만 설치되어 있으면 별도 SDK 설치 없이 바로
열고 빌드할 수 있습니다.

## 여는 방법

1. Visual Studio 2022에서 `Stm32WifiConfigTool.sln` 열기
2. F5(디버그 실행) 또는 Ctrl+Shift+B(빌드)

## 구성 (창 1개, 패널 6개 동시 표시)

MCU는 **USB(CDC 가상 COM)** 와 **UART(USART3, 보통 USB-시리얼 변환기 경유)** 두 채널에 항상
동일한 데이터를 미러링합니다. 두 채널 모두 `STX(0x02) + Data1,Data2,...,DataN + CR(0x0D) + LF(0x0A)`
프레임 포맷을 사용합니다(필드는 콤마로 구분, `docs/프로토콜_명세.md` §1). 이 도구는 두 채널을
완전히 독립적으로 연결·해제할 수 있고, 별도 팝업 창을 띄우지 않고 **메인 창 하나 안에서 여섯
패널을 항상 동시에** 볼 수 있게 도킹 배치했습니다: 좌상단 포트 설정, 중앙상단 WiFi 설정,
그 오른쪽 Measurement 설정, 그 오른쪽 RTC 설정, 우상단 ESP32 상태, 하단 전체 폭 측정값/상태 보기.

**패널 폭 조절**: 상단 5개 패널(포트/WiFi/Measurement 설정/RTC 설정/ESP32 상태) 사이 경계선에
마우스를 올리면 커서가 ↔ 모양으로 바뀝니다 — 그 상태로 드래그하면 각 패널의 폭을 원하는 대로
조절할 수 있습니다(`SplitContainer` 4개를 중첩해 구현, 너무 좁아져 내부 컨트롤이 잘리지 않도록
각 패널에 최소 폭이 걸려 있습니다). 조절한 폭은 **드래그를 놓는 즉시 설정 파일(`settings.ini`)에
바로 저장**되어(`MainForm.SaveSettingsSafe()`), 프로그램을 닫을 때까지 기다리지 않고도 다음 실행
시 그대로 복원됩니다 — 창의 X 버튼이 아니라 디버거 중지/작업 관리자 등으로 프로세스를 강제
종료해도 마지막으로 드래그를 놓은 시점의 폭은 이미 파일에 기록되어 있습니다
(`AppSettings.PortPanelWidth` / `WifiPanelWidth` / `MeasConfigPanelWidth` / `RtcPanelWidth`).

**폭이 복원되지 않을 때 확인할 것**: 폭을 조절한 뒤 `%AppData%\Stm32WifiConfigTool\settings.ini`
파일을 열어 `PortPanelWidth`/`WifiPanelWidth`/`MeasConfigPanelWidth`/`RtcPanelWidth` 값이 실제로
조절한 값으로 바뀌었는지 확인하세요. 바뀌어 있는데도 다음 실행 시 반영되지 않는다면 복원 로직
(`MainForm.MainForm_Load` → `ApplySavedSplitterDistances()`) 쪽 문제이고, 값 자체가 바뀌지 않는다면
저장이 안 되는 것이므로 원인이 다릅니다. 또한 Visual Studio에서 코드만 바꾸고 **다시 빌드하지
않은 채** 이전 실행 파일을 그대로 실행 중인 경우에도 같은 증상으로 보일 수 있으니, 솔루션을
완전히 다시 빌드한 뒤 테스트하세요.

하단 **측정값 보기** 패널(6번, 전체 폭) 내부에도 별도의 좌/우 스플리터가 하나 더 있습니다(좌:
측정값 그리드, 우: STATUS + 그 외 값 로그, `Panels/MeasurementPanel.cs`의 `_splitDisplay`) — 위
5개 패널과 같은 드래그·즉시 저장 방식이며 `AppSettings.MeasurementGridWidth`로 저장됩니다.

MCU가 보내는 비동기 메시지는 **측정값 프레임과 ESP32 상태 프레임을 구분해서 각각 다른 패널에
표시**합니다(둘 다 첫 필드로 구분되며, 응답 대기 로직도 이 둘을 명령 응답과 혼동하지 않도록
`Stm32Protocol.cs`의 화이트리스트 방식으로 분류합니다). 측정값 보기 패널 우측에는 이 둘 외에도
STATUS/EVENT/RESET_COUNT/커맨드 응답 등 측정값이 아닌 모든 프레임이 한데 모여 표시됩니다.

1. **포트 설정** (좌상단, `Panels/PortSettingsPanel.cs`)
   USB/UART 각각 COM 포트, Baud Rate, 읽기/쓰기 타임아웃(ms)을 설정하고 연결/해제합니다.
   다른 패널에서 명령을 보내거나 데이터를 받으려면 먼저 여기서 연결해야 합니다.
   - "새로고침": OS에 연결된 COM 포트 목록을 다시 읽어옵니다.
   - "지우기": 선택된 COM 포트를 비웁니다(연결 중에는 비활성화).
   - "연결"/"연결 해제": 포트를 열고 닫습니다.

2. **WiFi 설정** (중앙상단, `Panels/WifiConfigPanel.cs`)
   SSID/비밀번호, 서버 IP·Port, DHCP on/off, DHCP off일 때의 정적 IP/Gateway/Netmask를 설정합니다.
   - "Read": `WIFI_R_ALL` 프레임을 보내 MCU에 저장된 값을 한 번에 읽어와 화면에 채웁니다
     (비밀번호는 MCU가 `****`로 마스킹해서 돌려주므로 표시되지 않습니다 — 바꾸려면
     "비밀번호 변경" 체크 후 새로 입력).
   - "Write": 화면의 입력값 전체(SSID/Password/서버 IP·Port/DHCP/IP/Gateway/Mask)를
     `WIFI_W_ALL` 한 프레임에 담아 MCU에 전달합니다. "비밀번호 변경"을 체크하지 않았으면
     빈 문자열로 전송되며, 이 경우 MCU는 기존 저장된 비밀번호를 유지해야 합니다
     (`Models/NetConfig.cs`의 `Password` 필드 설명 참고). 저장되면 MCU가 자동으로
     WiFi/서버 재접속을 시도합니다.
   - 상단 "명령 전송 채널"에서 USB/UART 중 커맨드를 보낼 채널을 고릅니다.

3. **Measurement 설정** (WiFi 설정 오른쪽, `Panels/MeasurementConfigPanel.cs`, 신규)
   측정 모듈의 Reference(mV, 측정 상한치) / Offset(mV, 상한치 초과 시 노이즈 여유값) /
   Resistance(mOhm, 선간 저항 측정값) / Interval Time(sec, 측정 간격)을 설정합니다.
   - "Read": `MEAS_R_ALL` 프레임으로 현재값을 읽어와 화면에 채웁니다.
   - "Write": 입력값 전체를 `MEAS_W_ALL` 한 프레임에 담아 MCU에 전달합니다.
   - WiFi 설정 패널과 마찬가지로 "명령 전송 채널"/"커맨드 타임아웃"을 별도로 갖습니다.

4. **RTC 설정** (Measurement 설정 오른쪽, `Panels/RtcConfigPanel.cs`, 신규)
   RTC Wakeup Timer 기반 주기적 리셋 간격(초)을 설정합니다(`docs/프로토콜_명세.md` §6).
   시스템은 계속 동작하다가 이 주기가 되면 자동으로 리셋되고, 리셋마다 USART3/USB로
   `RESET_COUNT,<count>` 프레임을 1회 브로드캐스트합니다(누적 리셋 횟수 모니터링용 — 이
   패널 자체는 설정값 Read/Write만 다루며, `RESET_COUNT` 브로드캐스트는 별도로 확인하려면
   포트 설정 패널의 원시 수신 로그나 터미널 프로그램을 이용하세요).
   - "Read": `RESET_R_ALL` 프레임으로 현재 설정된 리셋 주기(초)를 읽어와 화면에 채웁니다.
   - "Write": 입력한 리셋 주기(초, 1~65536)를 `RESET_W_ALL` 한 프레임에 담아 MCU에 전달합니다.
     성공 시 MCU가 즉시 플래시에 저장하고 Wakeup Timer를 새 값으로 재무장합니다.
   - WiFi/Measurement 설정 패널과 마찬가지로 "명령 전송 채널"/"커맨드 타임아웃"을 별도로 갖습니다.
   - **이 커맨드는 `firmware/`·`firmware-no-rtos/` 양쪽 모두 이미 구현되어 있습니다**
     (아래 WIFI_R_ALL/MEAS_R_ALL 계열과 달리 실제 MCU와 바로 통신됩니다).

5. **ESP32 상태 보기** (우상단, `Panels/EspStatusPanel.cs`)
   MCU가 2초 간격으로 자동 브로드캐스트하는 `STATUS,<번호>` 프레임을 표시합니다. 측정값 프레임
   전송 사이사이에 도착하며, 측정값 패널과는 별도로 여기서만 다룹니다.
   - 현재 상태를 큰 글씨로(색상: DOWN=빨강, WIFI_UP=주황, TCP_UP=초록) 표시.
   - "표시 채널": USB / UART / 둘 다.
   - 하단에 수신 이력(시각/채널/번호/텍스트)을 로그로 쌓고, "지우기"로 초기화합니다.

6. **측정값 보기** (하단, 전체 폭, `Panels/MeasurementPanel.cs`)
   화면이 좌/우로 나뉘어 있습니다(경계선을 드래그해 폭 조절 가능, 조절한 폭은
   `AppSettings.MeasurementGridWidth`로 저장되어 재시작 후에도 유지됩니다):
   - **좌측 — 측정값 그리드**: FPGA 트리거 결과로 MCU가 보내는 `DC_<dc_ip>,<mac>,data1,...,dataN`
     프레임만 표로 보여줍니다. 첫 필드가 리터럴 `DC_` 접두어로 시작하는지로 식별하며(샘플 개수
     N은 고정이 아닙니다 — 실측 결과 6개가 아니라 12개까지 관측되어, 이 접두어 기준으로만
     판별하도록 되어 있습니다. `docs/프로토콜_명세.md` §2가 문서화한 "태그 없음/6개 고정"
     포맷과는 실제 다르니 유의하세요). `DC IP`/`MAC`은 이 장치(ESP32)의 station IP/MAC
     주소로, 측정값을 보낸 장치를 구분하는 용도입니다(그리드에는 `DC_` 접두어를 뗀 IP만 표시).
   - **우측 — 그 외 모든 값**: 좌측 측정값 그리드에 표시되지 않는 나머지 프레임을 전부 보여줍니다.
     - 위쪽 **STATUS** 로그: `STATUS,<번호>` 프레임만 골라 `STATUS:<번호>` 형태로 표시합니다
       (예: `STATUS:3`). ESP32 상태 번호 자체는 4번 패널(ESP32 상태 보기)에서도 큰 글씨로
       별도로 보이지만, 여기서는 수신 이력을 시간순으로 훑어볼 수 있습니다.
     - 아래쪽 일반 로그: `EVENT,WIFI_DISCONNECTED` / `EVENT,WIFI_CONNECTED` / `EVENT,TCP_CONNECTED` /
       `EVENT,TCP_CLOSED` 등 MCU의 비동기 알림, `RESET_COUNT,<count>`(RTC 리셋마다 1회, §6),
       그리고 다른 패널이 보낸 커맨드에 대한 응답 프레임(`WIFI_R_ALL,...`/`MEAS_R_ALL,...`/
       `RESET_R_ALL,...`/`ERR,...` 등, 같은 채널에 붙어 있는 모든 패널이 라인을 함께 받으므로)까지
       원본 필드를 콤마로 이어붙인 텍스트로 모두 표시됩니다.
   - "표시 채널": USB / UART / 둘 다 — 어느 채널에서 온 데이터를 그릴지 선택(좌/우 모두 동일하게 적용).
   - "자동 스크롤": 새 측정값이 들어올 때마다 그리드를 자동으로 맨 아래로 스크롤합니다.
   - "지우기": 그리드와 STATUS 로그, 일반 로그를 모두 비웁니다.
   - "CSV로 저장": 현재까지 쌓인 측정값을 CSV 파일로 내보냅니다.

## Visual Studio 디자이너로 폼/패널 편집하기

6개 패널(`PortSettingsPanel`, `WifiConfigPanel`, `MeasurementConfigPanel`, `RtcConfigPanel`,
`EspStatusPanel`, `MeasurementPanel`)과 `SerialChannelPanel`(USB/UART 공용 하위 컨트롤), `MainForm`은
모두 **표준 WinForms 디자이너
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

포트 설정(각 채널의 COM 포트/Baud Rate/읽기·쓰기 타임아웃), WiFi 설정·Measurement 설정·RTC 설정
패널의 명령 전송 채널·커맨드 타임아웃, 측정값 보기 패널의 표시 채널·자동 스크롤 여부, ESP32 상태
보기 패널의 표시 채널은 프로그램을 닫을 때 자동으로 다음 위치에 저장되고, 다음 실행 시 그대로
복원됩니다:

```
%AppData%\Stm32WifiConfigTool\settings.ini
```

`Services/AppSettingsStore.cs`가 담당하며, 외부 패키지 없이 단순 `key=value` 텍스트 형식이라
필요하면 직접 열어 수정해도 됩니다. 파일이 없거나 손상된 경우 기본값으로 안전하게 대체합니다.

**SSID/비밀번호/서버 IP 등 WiFi 접속 정보는 이 파일에 저장하지 않습니다** — 그 값들의 원본은
MCU의 W25Q40 플래시이므로, WiFi 설정 패널의 "Read"(`WIFI_R_ALL`)로 항상 MCU에서 다시 조회합니다.
특히 비밀번호를 PC에 평문으로 남기지 않기 위한 의도적인 설계입니다.

## 프로젝트 구조

```
Stm32WifiConfigTool/
  Stm32WifiConfigTool.csproj
  Program.cs               - 진입점
  MainForm.cs / .Designer.cs  - 단일 메인 창, 6개 패널을 도킹 배치 + ConnectionManager/설정 소유
  Panels/                   - UserControl(디자이너 지원), MainForm에 모두 도킹되어 표시
    SerialChannelPanel.cs / .Designer.cs  - USB 또는 UART 채널 1개의 연결 UI (PortSettingsPanel이 2개 사용)
    PortSettingsPanel.cs / .Designer.cs
    WifiConfigPanel.cs / .Designer.cs
    MeasurementConfigPanel.cs / .Designer.cs  - Reference/Offset/Resistance/Interval Time 설정
    RtcConfigPanel.cs / .Designer.cs      - RTC Wakeup Timer 리셋 주기(초) 설정 (신규)
    EspStatusPanel.cs / .Designer.cs      - ESP32 상태(STATUS,<번호>) 전용 패널
    MeasurementPanel.cs / .Designer.cs
  Services/
    LinkChannel.cs          - Usb/Uart 채널 구분
    SerialLinkService.cs    - 시리얼 연결 1개(연결/해제, 라인 단위 수신, 타임아웃)
    ConnectionManager.cs    - Usb/Uart SerialLinkService 2개를 앱 전체에서 공유
    Stm32Protocol.cs        - STX+CSV+CRLF 프레임 빌더/파서, 메시지 종류 분류(화이트리스트)
    Stm32Commands.cs        - WIFI_R_ALL/WIFI_W_ALL/MEAS_R_ALL/MEAS_W_ALL/RESET_R_ALL/RESET_W_ALL
                              async 요청-응답 헬퍼
    AppSettingsStore.cs     - PC측 UI 설정 로드/저장 (%AppData%\Stm32WifiConfigTool\settings.ini)
  Models/
    NetConfig.cs
    MeasurementConfig.cs    - Reference/Offset/Resistance/Interval Time
    RtcConfig.cs            - RTC 리셋 주기(초) (신규)
    MeasurementRecord.cs    - DC IP/MAC + data1..N (개수 가변)
    AppSettings.cs          - 저장 대상 설정 모델 (ChannelSettings 등)
```

## 프로토콜 참고

`docs/프로토콜_명세.md` §1의 STX+CSV+CRLF 프레이밍, §2 측정값 포맷, ESP32 상태(STATUS,<번호>)
프레임은 리포지토리 공용 규격을 그대로 따릅니다. **WIFI_R_ALL/WIFI_W_ALL/MEAS_R_ALL/MEAS_W_ALL은
이 PC 도구에서 새로 도입한 커맨드로, 이 시점 기준 `firmware/`·`firmware-no-rtos/`(MCU 쪽)에는
아직 구현되어 있지 않습니다** — 프레임 형태(필드 순서/개수, 응답 형식)는 `Services/Stm32Protocol.cs`의
`BuildWifiWriteAll`/`BuildMeasWriteAll` 및 `Stm32Commands.cs`의 `GetWifiAllAsync` 등의 XML 주석에
정의되어 있으니, MCU 측 `pc_comm.c`를 이 형식에 맞춰 구현해야 실제 통신이 됩니다(기존 SET/SAVE/
GET,CONFIG/STATUS 커맨드 방식은 이 도구에서 제거되었습니다). 반대로 **`RESET_R_ALL`/`RESET_W_ALL`
(§6, RTC 설정 패널)은 `firmware/`·`firmware-no-rtos/` 양쪽 `pc_comm.c`에 이미 구현되어 있어**
바로 실제 MCU와 통신됩니다. MCU 쪽 커맨드 파서는 `firmware/Core/Src/pc_comm.c`, 측정값 송신은
`firmware/Core/Src/fpga_link.c`, ESP32 상태 브로드캐스트/IP·MAC 조회는
`firmware/Core/Src/app_freertos.c`/`firmware/Core/Src/esp32_at.c`, RTC Wakeup Timer는
`firmware/Core/Src/rtc_wakeup.c` 참고.

## 알려진 제한사항

- 한 번에 하나의 커맨드-응답만 진행한다고 가정합니다(버튼 클릭이 겹치지 않는 일반적인 사용 기준).
  같은 채널에 대해 WiFi 설정 패널에서 커맨드 전송 중일 때 동시에 또 다른 커맨드를 보내면
  응답이 뒤섞일 수 있습니다.
- `SerialLinkService`의 읽기 루프는 설정된 읽기 타임아웃으로 폴링합니다. 타임아웃을 너무 짧게
  주면 CPU 사용량이 늘고, 너무 길게 주면 연결 해제 감지가 느려질 수 있습니다(기본값 3000ms 권장).
