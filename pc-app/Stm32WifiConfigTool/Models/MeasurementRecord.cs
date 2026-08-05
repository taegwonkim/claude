using System;

namespace Stm32WifiConfigTool.Models
{
    /// <summary>
    /// "DATA &lt;seq&gt; &lt;timestamp_ms&gt; &lt;sample0&gt; ..." 라인 1개를 파싱한 결과
    /// (docs/프로토콜_명세.md §2).
    /// </summary>
    public class MeasurementRecord
    {
        public DateTime ReceivedAt { get; set; }
        public string SourceChannel { get; set; } = string.Empty;
        public uint Seq { get; set; }
        public uint TimestampMs { get; set; }
        public int[] Samples { get; set; } = Array.Empty<int>();
        public string RawLine { get; set; } = string.Empty;

        public string SamplesText
        {
            get { return string.Join(", ", Samples); }
        }
    }
}
