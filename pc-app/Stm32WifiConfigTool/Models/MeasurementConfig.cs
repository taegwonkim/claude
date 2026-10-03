namespace Stm32WifiConfigTool.Models
{
    /// <summary>
    /// 측정 모듈 설정값. MEAS_R_ALL/MEAS_W_ALL 프레임으로 MCU와 주고받는다.
    /// </summary>
    public class MeasurementConfig
    {
        /// <summary>측정 상한치 (mV).</summary>
        public double ReferenceMv { get; set; }

        /// <summary>상한치를 넘을 시 노이즈를 고려한 여유값 (mV).</summary>
        public double OffsetMv { get; set; }

        /// <summary>선간 저항 측정값 (mOhm).</summary>
        public double ResistanceMOhm { get; set; }

        /// <summary>측정 간격 (sec).</summary>
        public double IntervalSec { get; set; }
    }
}
