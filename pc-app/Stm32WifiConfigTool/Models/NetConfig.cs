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
        /// 따라서 이 필드는 오직 "이번 세션에서 사용자가 새로 입력한 값"만 담으며, 비어 있으면
        /// WIFI_W_ALL 프레임에 빈 필드로 실려 전송된다 — MCU 쪽에서 "PASS 필드가 비어 있으면
        /// 기존 저장값을 유지"하는 규칙으로 처리해야 한다(WIFI_W_ALL은 SET처럼 필드 단위로 생략할
        /// 수 없는 all-in-one 프레임이므로, 이 관례를 MCU 구현에도 반드시 반영할 것).</summary>
        public string Password { get; set; } = string.Empty;

        public string ServerIp { get; set; } = string.Empty;
        public int ServerPort { get; set; } = 50001;
        public bool DhcpEnabled { get; set; } = true;
        public string StaticIp { get; set; } = string.Empty;
        public string Gateway { get; set; } = string.Empty;
        public string Netmask { get; set; } = string.Empty;
    }
}
