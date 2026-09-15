namespace Stm32WifiConfigTool.Models
{
    /// <summary>
    /// RTC Wakeup Timer 리셋 주기(초) 설정값. RESET_R_ALL/RESET_W_ALL 프레임으로 MCU와 주고받는다
    /// (docs/프로토콜_명세.md §6, firmware/firmware-no-rtos 양쪽 모두 이미 구현되어 있는 커맨드다).
    /// RTC_R_H/RTC_W_H, RTC_R_M/RTC_W_M, RTC_R_S/RTC_W_S로 개별로 읽고 쓰는 시/분/초 값(이 값과는
    /// 완전히 별도)은 <see cref="Panels.RtcConfigPanel"/>이 콤보박스 선택값(int)을 그대로 주고받으며,
    /// 별도의 모델 없이 <see cref="Services.Stm32Commands.GetRtcHourAsync"/> 등을 직접 호출한다.
    /// </summary>
    public class RtcConfig
    {
        /// <summary>리셋 주기(초). MCU 쪽 허용 범위는 1~65536(APP_RESET_MIN/MAX_PERIOD_SEC).</summary>
        public int PeriodSec { get; set; }
    }
}
