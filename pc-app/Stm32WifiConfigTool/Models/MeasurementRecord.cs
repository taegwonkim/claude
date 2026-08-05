using System;

namespace Stm32WifiConfigTool.Models
{
    /// <summary>
    /// "&lt;DC IP&gt;,&lt;MAC&gt;,data1,...,data6" 프레임 1개를 파싱한 결과
    /// (docs/프로토콜_명세.md §1-2, "DATA" 태그 없이 DC IP/MAC으로 시작).
    /// </summary>
    public class MeasurementRecord
    {
        public DateTime ReceivedAt { get; set; }
        public string SourceChannel { get; set; } = string.Empty;

        /// <summary>측정값을 보낸 장치(STM32+ESP32 유닛)의 ESP32 station IP.</summary>
        public string DcIp { get; set; } = string.Empty;

        /// <summary>측정값을 보낸 장치의 ESP32 station MAC 주소.</summary>
        public string MacAddress { get; set; } = string.Empty;

        /// <summary>data1..data6 (정확히 6개).</summary>
        public int[] Samples { get; set; } = Array.Empty<int>();
        public string RawLine { get; set; } = string.Empty;

        public string SamplesText
        {
            get { return string.Join(", ", Samples); }
        }
    }
}
