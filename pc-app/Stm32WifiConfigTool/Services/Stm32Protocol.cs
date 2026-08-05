using System;
using System.Collections.Generic;
using System.Globalization;
using Stm32WifiConfigTool.Models;

namespace Stm32WifiConfigTool.Services
{
    /// <summary>
    /// PC ↔ MCU 텍스트 라인 프로토콜 빌더/파서 (docs/프로토콜_명세.md §1-2).
    /// 순수 문자열 변환만 담당하며, 시리얼 송수신/타이밍은 <see cref="Stm32Commands"/>가 처리한다.
    /// </summary>
    public static class Stm32Protocol
    {
        public const string CmdSave = "SAVE";
        public const string CmdGetConfig = "GET CONFIG";
        public const string CmdStatus = "STATUS";
        public const string CmdHelp = "HELP";

        public static string BuildSetSsid(string ssid) => "SET SSID=" + ssid;
        public static string BuildSetPass(string pass) => "SET PASS=" + pass;
        public static string BuildSetServerIp(string ip) => "SET SERVER_IP=" + ip;
        public static string BuildSetServerPort(int port) => "SET SERVER_PORT=" + port.ToString(CultureInfo.InvariantCulture);
        public static string BuildSetDhcp(bool on) => "SET DHCP=" + (on ? "ON" : "OFF");
        public static string BuildSetIp(string ip) => "SET IP=" + ip;
        public static string BuildSetGateway(string gateway) => "SET GATEWAY=" + gateway;
        public static string BuildSetMask(string mask) => "SET MASK=" + mask;

        public static bool IsDataLine(string line) => line.StartsWith("DATA ", StringComparison.Ordinal);
        public static bool IsEventLine(string line) => line.StartsWith("EVENT ", StringComparison.Ordinal);
        public static bool IsErrorLine(string line) => line.StartsWith("ERR", StringComparison.Ordinal);

        /// <summary>"DATA &lt;seq&gt; &lt;timestamp_ms&gt; &lt;sample0&gt; ..." 한 줄을 파싱한다.</summary>
        public static bool TryParseDataLine(string line, string sourceChannel, out MeasurementRecord record)
        {
            record = null;
            if (!IsDataLine(line))
            {
                return false;
            }

            string[] parts = line.Split(new[] { ' ' }, StringSplitOptions.RemoveEmptyEntries);
            if (parts.Length < 3)
            {
                return false;
            }

            if (!uint.TryParse(parts[1], NumberStyles.Integer, CultureInfo.InvariantCulture, out uint seq))
            {
                return false;
            }
            if (!uint.TryParse(parts[2], NumberStyles.Integer, CultureInfo.InvariantCulture, out uint ts))
            {
                return false;
            }

            var samples = new List<int>();
            for (int i = 3; i < parts.Length; i++)
            {
                if (int.TryParse(parts[i], NumberStyles.Integer, CultureInfo.InvariantCulture, out int v))
                {
                    samples.Add(v);
                }
            }

            record = new MeasurementRecord
            {
                ReceivedAt = DateTime.Now,
                SourceChannel = sourceChannel,
                Seq = seq,
                TimestampMs = ts,
                Samples = samples.ToArray(),
                RawLine = line
            };
            return true;
        }

        /// <summary>"KEY=VALUE" 한 줄을 분리한다 (GET CONFIG 응답, STATUS 파싱용 보조).</summary>
        public static bool TryParseKeyValue(string line, out string key, out string value)
        {
            int eq = line.IndexOf('=');
            if (eq <= 0)
            {
                key = null;
                value = null;
                return false;
            }
            key = line.Substring(0, eq);
            value = line.Substring(eq + 1);
            return true;
        }
    }
}
