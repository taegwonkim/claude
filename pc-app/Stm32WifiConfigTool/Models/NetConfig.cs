namespace Stm32WifiConfigTool.Models
{
    /// <summary>
    /// MCU가 W25Q40 플래시에 저장하는 WiFi/서버 설정 (docs/프로토콜_명세.md §5의 PC 측 사본).
    /// GET CONFIG 응답으로 채워지며, SET+SAVE로 MCU에 반영한다.
    /// </summary>
    public class NetConfig
    {
        public string Ssid { get; set; } = string.Empty;

        /// <summary>MCU는 GET CONFIG에서 비밀번호를 "****"로 마스킹해 절대 평문으로 돌려주지 않는다.
        /// 따라서 이 필드는 오직 "이번 세션에서 사용자가 새로 입력한 값"만 담으며, 비어 있으면
        /// SetConfigAsync가 SET PASS 커맨드 자체를 보내지 않아 MCU에 저장된 기존 값이 유지된다.</summary>
        public string Password { get; set; } = string.Empty;

        public string ServerIp { get; set; } = string.Empty;
        public int ServerPort { get; set; } = 50001;
        public bool DhcpEnabled { get; set; } = true;
        public string StaticIp { get; set; } = string.Empty;
        public string Gateway { get; set; } = string.Empty;
        public string Netmask { get; set; } = string.Empty;
    }
}
