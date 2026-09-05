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

> **알려진 버그 수정**: WiFi 설정 패널만 폭을 조절해도 재실행 시 원래 크기로 돌아가는 문제가
> 있었습니다. 원인은 `_splitWifiMeas`(포트/WiFi 스플리터의 오른쪽 나머지를 다시 WiFi | 나머지로
> 나누는 스플리터)가 다른 3개와 달리 `FixedPanel = Panel2`로 되어 있었던 것 — 이 설정은 "창
> 크기가 바뀌면 WiFi 쪽(Panel1)이 늘어나고 줄어드는 나머지(Panel2)는 고정 폭을 유지"하는
> 뜻이라, 창 크기 복원 등으로 창이 리사이즈될 때마다 `WifiPanelWidth`로 복원해둔 값이 매번
> 자동으로 재계산되어 덮어써졌습니다. 다른 3개 스플리터처럼 `FixedPanel = Panel1`로 바꿔서
> WiFi 폭 자체가 불변으로 유지되고 나머지(Measurement/RTC/ESP32 상태 쪽)가 창 크기 변화를
> 흡수하도록 고쳤습니다.
>
> 위 수정과 별개로, 창/패널을 **줄이는** 방향으로는 잘 안 움직이던 문제도 있었습니다(늘리는
> 건 되는데 줄이는 게 안 됨). 각 `SplitContainer`의 `Panel1MinSize`/`Panel2MinSize`가 필요
> 이상으로 크게 잡혀 있었던 데다, 중첩된 스플리터의 실제 최소 폭 합계보다 바깥쪽 스플리터의
> `Panel2MinSize`가 더 작게 선언되어 있어(예: `_splitWifiMeas.Panel2MinSize`가 460인데 그
> 안에 중첩된 `_splitMeasStatus` 계열이 실제로 필요로 하는 최소 폭은 632였음) 서로 앞뒤가
> 안 맞았습니다. 각 패널의 실제 최소 폭을 다시 계산해 필요한 만큼만 요구하도록 줄이고
> (Port 280 / WiFi 300 / Measurement 200 / RTC 180 / ESP32 상태 200), 중첩된 스플리터를
> 감싸는 바깥쪽 스플리터의 `Panel2MinSize`는 안쪽 스플리터가 실제로 필요로 하는 최소 폭
> 합계와 정확히 일치하도록 다시 계산했습니다. `MainForm.MinimumSize`도 (1816, 780)에서
> (1220, 700)으로 낮춰서 창 자체도 더 작게 줄일 수 있습니다.

**패널을 좁힐 때 입력란도 함께 줄어듭니다**: 각 패널 안의 텍스트박스/콤보박스/NumericUpDown
입력 필드(SSID, 서버 IP, Reference/Offset/Resistance/Interval, 리셋 주기, 포트/Baud Rate/
타임아웃 등)는 `Anchor = Top|Left|Right`로 설정되어 있어, 패널 폭이 줄어들면 필드 폭도 함께
줄어들고(넓어지면 함께 넓어짐) 오른쪽 여백이 남지 않습니다. WiFi 설정 패널의 "비밀번호 변경"
체크박스처럼 필드 오른쪽에 나란히 있는 컨트롤은 `Anchor = Top|Right`로 반대쪽(오른쪽 테두리)에
고정해 필드가 넓어져도 겹치지 않게 했습니다. 왼쪽의 짧은 캡션 라벨(SSID/Baud Rate 등)은 크기가
고정이라 그대로 두었습니다. `Anchor`는 (앞서 도입한) `Location`+`Size` 자유 배치와 함께 써도
Visual Studio 디자이너의 드래그 크기 조절 핸들을 막지 않으므로, 여전히 디자이너에서 위치/크기를
자유롭게 편집할 수 있습니다.

**폭이 복원되지 않을 때 확인할 것**: 폭을 조절한 뒤 `%AppData%\Stm32WifiConfigTool\settings.ini`
파일을 열어 `PortPanelWidth`/`WifiPanelWidth`/`MeasConfigPanelWidth`/`RtcPanelWidth` 값이 실제로
조절한 값으로 바뀌었는지 확인하세요. 바뀌어 있는데도 다음 실행 시 반영되지 않는다면 복원 로직
(`MainForm.MainForm_Load` → `ApplySavedSplitterDistances()`) 쪽 문제이고, 값 자체가 바뀌지 않는다면
저장이 안 되는 것이므로 원인이 다릅니다. 또한 Visual Studio에서 코드만 바꾸고 **다시 빌드하지
않은 채** 이전 실행 파일을 그대로 실행 중인 경우에도 같은 증상으로 보일 수 있으니, 솔루션을
완전히 다시 빌드한 뒤 테스트하세요.

하단 **측정값 보기** 패널(6번, 전체 폭) 내부에도 별도의 좌/우 스플리터가 하나 더 있습니다(좌:
측정값 그리드, 우: 그 외 값 로그, `Panels/MeasurementPanel.cs`의 `_splitDisplay`) — 위
5개 패널과 같은 드래그·즉시 저장 방식이며 `AppSettings.MeasurementGridWidth`로 저장됩니다.

MCU가 보내는 비동기 메시지는 **측정값 프레임과 ESP32 상태 프레임을 구분해서 각각 다른 패널에
표시**합니다(둘 다 첫 필드로 구분되며, 응답 대기 로직도 이 둘을 명령 응답과 혼동하지 않도록
`Stm32Protocol.cs`의 화이트리스트 방식으로 분류합니다). 측정값 보기 패널 우측에는 이 둘 외에도
STATUS/EVENT/RESET_COUNT/커맨드 응답 등 측정값이 아닌 모든 프레임이 한데 모여 표시됩니다.

1. **포트 설정** (좌상단, `Panels/PortSettingsPanel.cs`)
   USB/UART 각각 COM 포트, Baud Rate, 읽기/쓰기 타임아웃(ms)을 설정하고 연결/해제합니다.
   다른 패널에서 명령을 보내거나 데이터를 받으려면 먼저 여기서 연결해야 합니다.
   - "새로고침": OS에 연결된 COM 포트 목록을 다시 읽어옵니다.
   - "연결"/"연결 해제": 포트를 열고 닫습니다.

2. **WiFi 설정** (중앙상단, `Panels/WifiConfigPanel.cs`)
   SSID/비밀번호, 서버 IP·Port, DHCP on/off, DHCP off일 때의 정적 IP/Gateway/Netmask를 설정합니다.
   - "Read": `WIFI_R_ALL` 프레임을 보내 MCU에 저장된 값을 한 번에 읽어와 화면에 채웁니다
     (비밀번호는 MCU가 `****`로 마스킹해서 돌려주므로 표시되지 않습니다 — 바꾸려면
     "비밀번호 변경" 체크 후 새로 입력). **실측된 필드 순서는 `SSID,Password,Server IP,
     Server Port,DHCP[,IP,Gateway,Netmask]`이며, DHCP는 `1`=사용/`0`=미사용으로 인코딩됩니다**
     (문서가 처음 가정했던 `ON`/`OFF` 텍스트가 아닙니다). **정적 IP/Gateway/Netmask 3개
     필드는 DHCP가 `0`(미사용)일 때만 DHCP 필드 뒤에 이어서 옵니다** — DHCP가 `1`(사용)이면
     이 3개 필드 자체가 응답에 없습니다(당연히 안 쓰는 값이니 안 보내는 것 — `Stm32Commands.
     GetWifiAllAsync` 참고). DHCP=1이라 이 필드들이 없을 때는 그 세 입력란을 건드리지 않고
     화면에 있던 값(직전 캐시 또는 사용자가 입력한 값)을 그대로 둡니다. DHCP=0인데 정적 IP
     필드가 없으면 프로토콜 위반으로 보고 오류를 표시합니다.
   - "Write": 화면의 입력값(SSID/Password/서버 IP·Port/DHCP)을 `WIFI_W_ALL` 한 프레임에 담아
     MCU에 전달합니다. DHCP는 동일하게 `1`/`0`으로 인코딩하며, **IP/Gateway/Mask 3개 필드는
     DHCP를 사용하지 않을 때(정적 IP 모드)만 DHCP 필드 뒤에 덧붙여 전송**합니다(DHCP 사용
     시에는 이 3개 필드 자체를 보내지 않아 응답과 대칭을 이룹니다). "비밀번호 변경"을
     체크하지 않았으면 빈 문자열로 전송되며, 이 경우 MCU는 기존 저장된 비밀번호를 유지해야
     합니다(`Models/NetConfig.cs`의 `Password` 필드 설명 참고). 저장되면 MCU가 자동으로
     WiFi/서버 재접속을 시도합니다.
   - 상단 "명령 전송 채널"에서 USB/UART 중 커맨드를 보낼 채널을 고릅니다.
   - **"Read"에 성공한 값(비밀번호 제외)은 로컬에 캐시되어 다음 실행 시 화면에 미리
     채워집니다**(`AppSettings.WifiSsidCache` 등, `WifiConfigPanel.Initialize()`/
     `SaveConfigCache()` 참고 — MCU 재조회 전 참고용일 뿐 원본은 항상 MCU이며, 비밀번호만
     예외적으로 캐시하지 않고 항상 빈 채로 시작합니다).
   - "커맨드 타임아웃(ms)"은 Read/Write 버튼 옆이 아니라 그 **아래 별도 줄**에 있습니다(패널
     폭이 좁아졌을 때 버튼과 겹치지 않도록 `_timeoutRow`라는 별도 행으로 분리했습니다). 이 행은
     `_fieldsGroup`과 같은 자유 배치(Location+Size) 방식으로 바꿔, 라벨은 x=15, 입력란은
     x=150에 위치시켜 위쪽 "설정값" 그룹의 라벨/입력란 열, 그리고 맨 위 "명령 전송 채널"
     그룹(라디오 버튼도 x=15에서 시작)과 세로로 한 줄에 맞춰지도록 했습니다.
   - **각 입력란 오른쪽 여백을 조절하려면**: `WifiConfigPanel.cs`의 `FieldRightMargin`
     상수(px) 하나만 바꾸면 됩니다. `ApplyFieldRightMargins()`가 이 값으로 SSID/서버 IP/서버
     Port/정적 IP/Gateway/Netmask 입력란과 "비밀번호 변경" 체크박스의 우측 여백을 한 번에
     다시 계산해 적용합니다(생성자에서 `InitializeComponent()` 직후 1회 호출). 디자이너
     (`WifiConfigPanel.Designer.cs`)의 고정 `Size`/`Location` 값은 디자인 타임 미리보기용일
     뿐이며, 실제 실행 시 여백은 이 상수가 결정합니다. 비밀번호 입력란과 그 오른쪽 "비밀번호
     변경" 체크박스 사이의 간격은 별도의 `PasswordCheckboxGap` 상수로 조절합니다.

3. **Measurement 설정** (WiFi 설정 오른쪽, `Panels/MeasurementConfigPanel.cs`, 신규)
   측정 모듈의 Reference(mV, 측정 상한치) / Offset(mV, 상한치 초과 시 노이즈 여유값) /
   Resistance(mOhm, 선간 저항 측정값) / Interval Time(sec, 측정 간격)을 설정합니다.
   - "Read": `MEAS_R_ALL` 프레임으로 현재값을 읽어와 화면에 채웁니다.
   - "Write": 입력값 전체를 `MEAS_W_ALL` 한 프레임에 담아 MCU에 전달합니다.
   - WiFi 설정 패널과 마찬가지로 "명령 전송 채널"/"커맨드 타임아웃"을 별도로 갖고, "Read" 값도
     동일하게 로컬 캐시되어 다음 실행 시 미리 채워집니다.

4. **RTC 설정** (Measurement 설정 오른쪽, `Panels/RtcConfigPanel.cs`, 신규)
   RTC Wakeup Timer 기반 주기적 리셋 간격(초)을 설정합니다(`docs/프로토콜_명세.md` §6).
   시스템은 계속 동작하다가 이 주기가 되면 자동으로 리셋되고, 리셋마다 USART3/USB로
   `RESET_COUNT,<count>` 프레임을 1회 브로드캐스트합니다(누적 리셋 횟수 모니터링용 — 이
   패널 자체는 설정값 Read/Write만 다루며, `RESET_COUNT` 브로드캐스트는 별도로 확인하려면
   포트 설정 패널의 원시 수신 로그나 터미널 프로그램을 이용하세요).
   - "Read": `RESET_R_ALL` 프레임으로 현재 설정된 리셋 주기(초)를 읽어와 화면에 채웁니다.
   - "Write": 입력한 리셋 주기(초, 1~65536)를 `RESET_W_ALL` 한 프레임에 담아 MCU에 전달합니다.
     성공 시 MCU가 즉시 플래시에 저장하고 Wakeup Timer를 새 값으로 재무장합니다.
   - WiFi/Measurement 설정 패널과 마찬가지로 "명령 전송 채널"/"커맨드 타임아웃"을 별도로 갖고,
     "Read" 값도 동일하게 로컬 캐시되어 다음 실행 시 미리 채워집니다.
   - **이 커맨드는 `firmware/`·`firmware-no-rtos/` 양쪽 모두 이미 구현되어 있습니다**
     (아래 WIFI_R_ALL/MEAS_R_ALL 계열과 달리 실제 MCU와 바로 통신됩니다).

5. **ESP32 상태 보기** (우상단, `Panels/EspStatusPanel.cs`)
   MCU가 2초 간격으로 자동 브로드캐스트하는 STATUS 프레임을 표시합니다. 측정값 프레임 전송
   사이사이에 도착하며, 측정값 패널과는 별도로 여기서만 다룹니다. `docs/프로토콜_명세.md` §1이
   문서화한 콤마 형식(`STATUS,<번호>`)과 실측된 콜론 형식(`STATUS:<번호>`) 둘 다 인식하고,
   STX(0x02) 유무도 가리지 않습니다(`Stm32Protocol.TryParseStatusText`/`DisplayText` 참고 —
   실측 결과 MCU가 모든 프레임에 STX를 붙이지는 않았습니다).
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
   - **우측 — 그 외 모든 값**: 좌측 측정값 그리드에 표시되지 않는 나머지를 한 로그에 원본
     그대로 모아 보여줍니다(높이 전체를 채움) — STX 유무나 태그 형식에 관계없이 전부 표시되며
     (`Stm32Protocol.DisplayText`가 STX를 있으면 떼고 없으면 그대로 두며, 빈 줄만 무시합니다 —
     실측 결과 MCU가 모든 프레임에 STX를 붙이지는 않았습니다), `STATUS,<번호>`/`STATUS:<번호>`,
     `EVENT,WIFI_DISCONNECTED` / `EVENT,WIFI_CONNECTED` / `EVENT,TCP_CONNECTED` / `EVENT,TCP_CLOSED`
     등 MCU의 비동기 알림, `RESET_COUNT,<count>`(RTC 리셋마다 1회, §6), 다른 패널이 보낸 커맨드에
     대한 응답 프레임(`WIFI_R_ALL,...`/`MEAS_R_ALL,...`/`RESET_R_ALL,...`/`ERR,...` 등, 같은
     채널에 붙어 있는 모든 패널이 라인을 함께 받으므로)까지 모두 포함됩니다. ESP32 상태 번호
     자체는 4번 패널(ESP32 상태 보기)에서 큰 글씨로 별도로 보이므로, 여기서는 STATUS만을 위한
     별도 칸을 두지 않습니다(중복 방지).
   - "표시 채널": USB / UART / 둘 다 — 어느 채널에서 온 데이터를 그릴지 선택(좌/우 모두 동일하게 적용).
   - "자동 스크롤": 새 측정값이 들어올 때마다 그리드를 자동으로 맨 아래로 스크롤합니다.
   - "지우기": 그리드와 우측 로그를 모두 비웁니다.
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

`docs/프로토콜_명세.md` §1의 STX+CSV+CRLF 프레이밍은 리포지토리 공용 규격을 그대로 따릅니다.

**중요 — 실제 장비의 커맨드 응답 형식이 이 저장소 문서/펌웨어와 다릅니다.** 실측 결과(2026-09,
실제 MCU 대상 테스트) 각 설정 커맨드(WIFI_/MEAS_/RESET_ 등)의 응답은 `docs/프로토콜_명세.md`가
가정한 "태그로 시작하는 응답"(예: `MEAS_R_ALL,5000,200,0,1`)이 아니라, **태그 없이 값만 맨몸으로**
오는 `<STX>data,...,<CR><LF>` 형태입니다 — 쓰기 응답은 그냥 `OK`, 읽기 응답은 그냥 `5000,200,0,1,0`
처럼 옵니다. 이는 지금 테스트 중인 MCU가 이 저장소의 `firmware/`·`firmware-no-rtos/` 코드를 그대로
쓰고 있지 않을 가능성을 시사합니다(이 저장소 펌웨어는 항상 태그를 붙여 응답합니다, 예:
`RESET_W_ALL,OK`).

이 차이 때문에 처음엔 MEAS_R_ALL/MEAS_W_ALL이 "응답 타임아웃"으로 실패했습니다(첫 필드가
커맨드명과 같은지로 응답 여부를 판별하던 화이트리스트 방식이라 태그 없는 응답을 아예 놓쳤음).
지금은 **`Stm32Protocol.IsBroadcastFrame`이 확실한 비동기 브로드캐스트(STATUS/EVENT/RESET_COUNT/
측정값(`DC_` 접두어))만 걸러내고, 그 나머지는 태그가 있든 없든 진행 중인 커맨드의 응답 후보로
취급**하도록 바꿨습니다. `Stm32Commands.cs`의 각 Get/Set 헬퍼도 `StripTag()`로 태그가 있으면 떼고
없으면 그대로 쓰는 방식이라, 이 저장소 펌웨어(태그 있음)와 실제 MCU(태그 없음) 양쪽 응답을 모두
받아들입니다.

**WIFI_R_ALL/WIFI_W_ALL/MEAS_R_ALL/MEAS_W_ALL은 이 PC 도구에서 새로 도입한 커맨드로, 이 시점
기준 `firmware/`·`firmware-no-rtos/`(이 저장소 MCU 쪽)에는 아직 구현되어 있지 않습니다** —
프레임 형태(필드 순서/개수, 응답 형식)는 `Services/Stm32Protocol.cs`의 `BuildWifiWriteAll`/
`BuildMeasWriteAll` 및 `Stm32Commands.cs`의 `GetWifiAllAsync` 등의 XML 주석에 정의되어 있으니,
MCU 측 `pc_comm.c`를 이 형식(태그 있는 응답이든 없는 응답이든 위 설명대로 둘 다 허용됨)에 맞춰
구현하면 실제 통신이 됩니다(기존 SET/SAVE/GET,CONFIG/STATUS 커맨드 방식은 이 도구에서 제거되었습니다).
반대로 **`RESET_R_ALL`/`RESET_W_ALL`(§6, RTC 설정 패널)은 `firmware/`·`firmware-no-rtos/` 양쪽
`pc_comm.c`에 이미 구현되어 있습니다**(단, 실제 테스트 중인 MCU가 이 저장소 펌웨어와 다르다면
이 커맨드도 마찬가지로 태그 없는 형태로 응답할 수 있습니다 — 위 설명대로 어느 쪽이든 동작합니다).
MCU 쪽 커맨드 파서는 `firmware/Core/Src/pc_comm.c`, 측정값 송신은 `firmware/Core/Src/fpga_link.c`,
ESP32 상태 브로드캐스트/IP·MAC 조회는 `firmware/Core/Src/app_freertos.c`/`firmware/Core/Src/esp32_at.c`,
RTC Wakeup Timer는 `firmware/Core/Src/rtc_wakeup.c` 참고.

## 알려진 제한사항

- 한 번에 하나의 커맨드-응답만 진행한다고 가정합니다(버튼 클릭이 겹치지 않는 일반적인 사용 기준).
  같은 채널에 대해 WiFi 설정 패널에서 커맨드 전송 중일 때 동시에 또 다른 커맨드를 보내면
  응답이 뒤섞일 수 있습니다.
- `SerialLinkService`의 읽기 루프는 설정된 읽기 타임아웃으로 폴링합니다. 타임아웃을 너무 짧게
  주면 CPU 사용량이 늘고, 너무 길게 주면 연결 해제 감지가 느려질 수 있습니다(기본값 3000ms 권장).
