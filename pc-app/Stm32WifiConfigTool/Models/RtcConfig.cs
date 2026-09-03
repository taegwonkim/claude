namespace Stm32WifiConfigTool.Models
{
    /// <summary>
    /// RTC Wakeup Timer 기반 주기적 리셋 설정값. RESET_R_ALL/RESET_W_ALL 프레임으로 MCU와 주고받는다
    /// (docs/프로토콜_명세.md §6). firmware/firmware-no-rtos 양쪽 모두 이미 구현되어 있는 커맨드다.
    /// </summary>
    public class RtcConfig
    {
        /// <summary>리셋 주기(초). MCU 쪽 허용 범위는 1~65536(APP_RESET_MIN/MAX_PERIOD_SEC).</summary>
        public int PeriodSec { get; set; }
    }
}
