using System;

namespace Stm32WifiConfigTool.Models
{
    /// <summary>
    /// "DC_&lt;dc_ip&gt;,&lt;mac&gt;,data1,...,dataN" 프레임 1개를 파싱한 결과 — 첫 필드의 "DC_"
    /// 접두어로 측정값 프레임임을 식별한다(<see cref="Services.Stm32Protocol.TryParseMeasurementRecord"/>).
    /// 샘플 개수(N)는 고정이 아니다(실측 결과 6개가 아니라 12개까지 관측됨).
    /// </summary>
    public class MeasurementRecord
    {
        public DateTime ReceivedAt { get; set; }
        public string SourceChannel { get; set; } = string.Empty;

        /// <summary>측정값을 보낸 장치(STM32+ESP32 유닛)의 ESP32 station IP ("DC_" 접두어는 제거된 값).</summary>
        public string DcIp { get; set; } = string.Empty;

        /// <summary>측정값을 보낸 장치의 ESP32 station MAC 주소.</summary>
        public string MacAddress { get; set; } = string.Empty;

        /// <summary>data1..dataN (개수는 프레임마다 다를 수 있음).</summary>
        public int[] Samples { get; set; } = Array.Empty<int>();
        public string RawLine { get; set; } = string.Empty;

        public string SamplesText
        {
            get { return string.Join(", ", Samples); }
        }
    }
}
