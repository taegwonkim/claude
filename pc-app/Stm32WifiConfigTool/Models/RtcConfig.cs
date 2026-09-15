namespace Stm32WifiConfigTool.Models
{
    /// <summary>
    /// RTC Wakeup Timer 관련 설정값. 두 가지 독립된 값을 함께 담는다:
    /// (1) <see cref="PeriodSec"/> - 기존 리셋 주기(초, 하나의 값), RESET_R_ALL/RESET_W_ALL
    /// 프레임으로 MCU와 주고받는다(docs/프로토콜_명세.md §6, firmware/firmware-no-rtos 양쪽 모두
    /// 이미 구현되어 있는 커맨드다).
    /// (2) <see cref="Hour"/>/<see cref="Minute"/>/<see cref="Second"/> - RTC_R_H/RTC_W_H,
    /// RTC_R_M/RTC_W_M, RTC_R_S/RTC_W_S 프레임으로 각각 개별로 읽고 쓰는 값. (1)과는 완전히
    /// 별도의 값이며 서로 자동으로 맞춰지지 않는다.
    /// </summary>
    public class RtcConfig
    {
        /// <summary>리셋 주기(초). MCU 쪽 허용 범위는 1~65536(APP_RESET_MIN/MAX_PERIOD_SEC).</summary>
        public int PeriodSec { get; set; }

        /// <summary>RTC_R_H/RTC_W_H로 개별로 읽고 쓰는 "시" 값.</summary>
        public int Hour { get; set; }

        /// <summary>RTC_R_M/RTC_W_M으로 개별로 읽고 쓰는 "분" 값.</summary>
        public int Minute { get; set; }

        /// <summary>RTC_R_S/RTC_W_S로 개별로 읽고 쓰는 "초" 값.</summary>
        public int Second { get; set; }
    }
}
