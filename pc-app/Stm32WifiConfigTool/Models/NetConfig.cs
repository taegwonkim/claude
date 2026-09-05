namespace Stm32WifiConfigTool.Models
{
    /// <summary>
    /// MCU가 W25Q40 플래시에 저장하는 WiFi/서버 설정 (docs/프로토콜_명세.md §5의 PC 측 사본).
    /// WIFI_R_ALL 응답으로 채워지며, WIFI_W_ALL 한 프레임으로 MCU에 반영한다.
    /// </summary>
    public class NetConfig
    {
        public string Ssid { get; set; } = string.Empty;

        /// <summary>MCU는 WIFI_R_ALL에서 비밀번호를 "****"로 마스킹해 절대 평문으로 돌려주지 않는다.
        /// WIFI_W_ALL은 SET처럼 필드 단위로 생략할 수 없는 all-in-one 프레임이므로, 원래는 이 필드가
        /// 비어 있으면 MCU가 "PASS 필드가 비어 있으면 기존 저장값 유지"로 처리할 것으로 가정했으나,
        /// 실측 결과 MCU가 빈 값을 그대로 저장해 비밀번호가 지워지는 것으로 확인되었다. 따라서 이
        /// 필드를 직접 만드는 호출자(<see cref="Panels.WifiConfigPanel.ReadConfigFromUi"/>)는 이제
        /// "비밀번호 변경"을 사용자가 체크하지 않았다면 빈 문자열이 아니라 이번 실행에서 마지막으로
        /// 보낸 값을 다시 채워 넣어, 다른 필드만 바꿔 Write해도 비밀번호가 지워지지 않게 한다.</summary>
        public string Password { get; set; } = string.Empty;

        public string ServerIp { get; set; } = string.Empty;
        public int ServerPort { get; set; } = 50001;
        public bool DhcpEnabled { get; set; } = true;
        public string StaticIp { get; set; } = string.Empty;
        public string Gateway { get; set; } = string.Empty;
        public string Netmask { get; set; } = string.Empty;
    }
}
