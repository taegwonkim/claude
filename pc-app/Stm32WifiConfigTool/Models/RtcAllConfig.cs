namespace Stm32WifiConfigTool.Models
{
    /// <summary>
    /// RTC 리셋 설정값. RTC_R_ALL/RTC_W_ALL 프레임("리셋 주기,단위,리셋 사용" 필드 순서)으로
    /// MCU와 주고받는다.
    /// </summary>
    public class RtcAllConfig
    {
        /// <summary>리셋 주기 (초).</summary>
        public int PeriodSec { get; set; }

        /// <summary>단위 코드 - "H"(시)/"M"(분)/"S"(초) 중 하나.</summary>
        public string UnitCode { get; set; } = "H";

        /// <summary>리셋 사용 여부.</summary>
        public bool ResetEnabled { get; set; }
    }
}
