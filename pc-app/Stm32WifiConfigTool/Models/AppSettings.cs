namespace Stm32WifiConfigTool.Models
{
    /// <summary>USB 또는 UART 채널 1개의 연결 설정(포트/보레이트/타임아웃).</summary>
    public class ChannelSettings
    {
        public string PortName { get; set; } = string.Empty;
        public int BaudRate { get; set; } = 115200;
        public int ReadTimeoutMs { get; set; } = 3000;
        public int WriteTimeoutMs { get; set; } = 2000;
    }

    /// <summary>
    /// 앱 재시작 후에도 유지할 PC측 UI 설정. AppSettingsStore가 로컬 파일로 저장/로드한다.
    /// WiFi/Measurement/RTC 설정 패널에서 "Read"에 성공한 값은 각 *Cache 필드에 저장되어 다음
    /// 실행 시 화면에 미리 채워진다(원본은 항상 MCU이고, 이 캐시는 마지막으로 확인한 값을
    /// 보여주기 위한 참고용일 뿐이다 - "Write"만 하고 "Read"는 하지 않으면 갱신되지 않는다).
    /// WiFi 비밀번호(<see cref="WifiPasswordCache"/>)도 내부망 전용 환경이라는 전제로 여기 함께
    /// 평문 캐시되며, 다음 실행 시 화면에 미리 채워진다(설정 파일 접근 권한 관리는 사용자 책임).
    /// </summary>
    public class AppSettings
    {
        public ChannelSettings Usb { get; set; } = new ChannelSettings();
        public ChannelSettings Uart { get; set; } = new ChannelSettings();

        /// <summary>WiFi 설정 패널에서 커맨드를 보낼 채널: "Usb" 또는 "Uart".</summary>
        public string WifiCommandChannel { get; set; } = "Usb";
        public int WifiCommandTimeoutMs { get; set; } = 3000;

        /// <summary>WiFi 설정 패널에서 마지막으로 "Read"한 값.</summary>
        public string WifiSsidCache { get; set; } = string.Empty;
        public string WifiServerIpCache { get; set; } = string.Empty;
        public int WifiServerPortCache { get; set; } = 50001;
        public bool WifiDhcpEnabledCache { get; set; } = true;
        public string WifiStaticIpCache { get; set; } = string.Empty;
        public string WifiGatewayCache { get; set; } = string.Empty;
        public string WifiNetmaskCache { get; set; } = string.Empty;

        /// <summary>WiFi 설정 패널에서 마지막으로 MCU에 성공적으로 전달한 비밀번호(평문). MCU가
        /// WIFI_R_ALL에서 비밀번호를 마스킹해 돌려주므로 "Read"로는 채울 수 없고, "Write" 성공
        /// 시에만 갱신된다(<see cref="Panels.WifiConfigPanel"/> 참고). 내부망 전용 환경이라는
        /// 전제로 캐시한다 - 공유 PC 등 다수가 접근 가능한 환경에서는 주의할 것.</summary>
        public string WifiPasswordCache { get; set; } = string.Empty;

        /// <summary>Measurement 설정 패널에서 커맨드를 보낼 채널: "Usb" 또는 "Uart".</summary>
        public string MeasConfigCommandChannel { get; set; } = "Usb";
        public int MeasConfigCommandTimeoutMs { get; set; } = 3000;

        /// <summary>Measurement 설정 패널에서 마지막으로 "Read"한 값.</summary>
        public double MeasReferenceMvCache { get; set; } = 0;
        public double MeasOffsetMvCache { get; set; } = 0;
        public double MeasResistanceMOhmCache { get; set; } = 0;
        public double MeasIntervalSecCache { get; set; } = 1;

        /// <summary>RTC(리셋 주기) 설정 패널에서 커맨드를 보낼 채널: "Usb" 또는 "Uart".</summary>
        public string RtcConfigCommandChannel { get; set; } = "Usb";
        public int RtcConfigCommandTimeoutMs { get; set; } = 3000;

        /// <summary>RTC 설정 패널에서 마지막으로 "Read"한 값.</summary>
        public int RtcPeriodSecCache { get; set; } = 3600;

        /// <summary>측정값 보기 패널의 표시 채널: "Usb" / "Uart". ("Both"였던 예전 설정 파일이
        /// 남아 있어도 "Usb"로 취급된다 - <see cref="Panels.MeasurementPanel.Initialize"/> 참고.)</summary>
        public string MeasurementDisplayChannel { get; set; } = "Usb";
        public bool MeasurementAutoScroll { get; set; } = true;

        /// <summary>측정값 보기 패널 내부의 좌(측정값 그리드)/우(STATUS + 그 외 수신값 로그) 스플리터
        /// 위치(px, 좌측 폭). 다른 패널 폭들과 마찬가지로 드래그 즉시 저장되고 재시작 후 복원된다.</summary>
        public int MeasurementGridWidth { get; set; } = 550;

        /// <summary>ESP32 상태 보기 패널의 표시 채널: "Usb" / "Uart". ("Both"였던 예전 설정 파일이
        /// 남아 있어도 "Usb"로 취급된다 - <see cref="Panels.EspStatusPanel.Initialize"/> 참고.)</summary>
        public string EspStatusDisplayChannel { get; set; } = "Usb";

        /// <summary>상단 5개 패널 사이 스플리터 위치(px). 사용자가 경계선을 드래그해 각 패널의
        /// 폭을 조절하면 실시간으로 갱신되고, 앱 재시작 후에도 유지된다.
        /// PortPanelWidth: 포트 설정 패널 폭. WifiPanelWidth: (전체 폭 - 포트 폭) 중 WiFi 설정이
        /// 차지하는 폭. MeasConfigPanelWidth: (WifiPanelWidth 오른쪽 나머지) 중 Measurement 설정이
        /// 차지하는 폭. RtcPanelWidth: (MeasConfigPanelWidth 오른쪽 나머지) 중 RTC 설정이 차지하는
        /// 폭. ESP32 상태 패널은 그 오른쪽에 남는 폭을 그대로 사용한다(별도 저장하지 않음).</summary>
        public int PortPanelWidth { get; set; } = 460;
        public int WifiPanelWidth { get; set; } = 840;
        public int MeasConfigPanelWidth { get; set; } = 300;
        public int RtcPanelWidth { get; set; } = 260;

        /// <summary>메인 창 크기(px). 0이면 아직 저장된 값이 없다는 뜻이며, 이 경우 MainForm.
        /// Designer.cs가 지정한 기본 크기를 그대로 사용한다(사용자가 창 크기를 한 번이라도
        /// 조절하거나 최대화하면 그 이후부터 저장되어 다음 실행 시 그대로 복원된다).</summary>
        public int WindowWidth { get; set; } = 0;
        public int WindowHeight { get; set; } = 0;
        public bool WindowMaximized { get; set; } = false;
    }
}
