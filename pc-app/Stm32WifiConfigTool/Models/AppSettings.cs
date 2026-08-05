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
    /// SSID/비밀번호/서버 IP 등 WiFi 접속 정보는 MCU 플래시에 저장되는 값이 원본(source of truth)이므로
    /// 여기에는 포함하지 않는다 - 특히 비밀번호를 PC에 평문으로 남기지 않기 위함.
    /// </summary>
    public class AppSettings
    {
        public ChannelSettings Usb { get; set; } = new ChannelSettings();
        public ChannelSettings Uart { get; set; } = new ChannelSettings();

        /// <summary>WiFi 설정 패널에서 커맨드를 보낼 채널: "Usb" 또는 "Uart".</summary>
        public string WifiCommandChannel { get; set; } = "Usb";
        public int WifiCommandTimeoutMs { get; set; } = 3000;

        /// <summary>측정값 보기 패널의 표시 채널: "Usb" / "Uart" / "Both".</summary>
        public string MeasurementDisplayChannel { get; set; } = "Both";
        public bool MeasurementAutoScroll { get; set; } = true;
    }
}
