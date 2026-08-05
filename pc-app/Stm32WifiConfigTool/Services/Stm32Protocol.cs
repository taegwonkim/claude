using System;
using System.Collections.Generic;
using System.Globalization;
using Stm32WifiConfigTool.Models;

namespace Stm32WifiConfigTool.Services
{
    /// <summary>
    /// PC ↔ MCU 프레임 프로토콜: STX(0x02) + Data1,Data2,...,DataN + CR(0x0D) + LF(0x0A).
    /// 필드는 콤마(',')로 구분한다 (docs/프로토콜_명세.md §1-2).
    /// SerialLinkService가 바이트 스트림을 '\n' 기준으로 이미 한 줄씩 잘라 트레일링 '\r'까지
    /// 제거해 넘겨주므로, 여기서는 그 문자열의 첫 글자가 STX인지 확인하고 나머지를 콤마로
    /// 나누기만 하면 된다. 순수 문자열 변환만 담당하며, 시리얼 송수신/타이밍은
    /// <see cref="Stm32Commands"/>가 처리한다.
    /// </summary>
    public static class Stm32Protocol
    {
        public const char Stx = '\x02';

        public static string BuildSetSsid(string ssid) => Stx + "SET,SSID," + ssid;
        public static string BuildSetPass(string pass) => Stx + "SET,PASS," + pass;
        public static string BuildSetServerIp(string ip) => Stx + "SET,SERVER_IP," + ip;
        public static string BuildSetServerPort(int port) => Stx + "SET,SERVER_PORT," + port.ToString(CultureInfo.InvariantCulture);
        public static string BuildSetDhcp(bool on) => Stx + "SET,DHCP," + (on ? "ON" : "OFF");
        public static string BuildSetIp(string ip) => Stx + "SET,IP," + ip;
        public static string BuildSetGateway(string gateway) => Stx + "SET,GATEWAY," + gateway;
        public static string BuildSetMask(string mask) => Stx + "SET,MASK," + mask;

        public static readonly string CmdSave = Stx + "SAVE";
        public static readonly string CmdGetConfig = Stx + "GET,CONFIG";
        public static readonly string CmdStatus = Stx + "STATUS";
        public static readonly string CmdHelp = Stx + "HELP";

        /// <summary>SerialLinkService.LineReceived로 전달된 한 줄을 파싱한다. 맨 앞이 STX가 아니면
        /// false(잡음/깨진 프레임 - 호출자는 무시해야 한다).</summary>
        public static bool TryParseFrame(string rawLine, out string[] fields)
        {
            if (string.IsNullOrEmpty(rawLine) || rawLine[0] != Stx)
            {
                fields = null;
                return false;
            }
            string payload = rawLine.Substring(1);
            fields = payload.Length == 0 ? new string[0] : payload.Split(',');
            return true;
        }

        public static bool IsDataFrame(string[] fields) => fields != null && fields.Length > 0 && fields[0] == "DATA";
        public static bool IsEventFrame(string[] fields) => fields != null && fields.Length > 0 && fields[0] == "EVENT";
        public static bool IsErrorFrame(string[] fields) => fields != null && fields.Length > 0 && fields[0] == "ERR";

        /// <summary>로그/메시지박스 표시용: 맨 앞 STX가 있으면 제거한다(없으면 원본 그대로).</summary>
        public static string DisplayText(string s)
        {
            return (!string.IsNullOrEmpty(s) && s[0] == Stx) ? s.Substring(1) : s;
        }

        /// <summary>"DATA,&lt;seq&gt;,&lt;timestamp_ms&gt;,&lt;sample0&gt;,..." 필드 배열을 측정값 레코드로 변환한다.</summary>
        public static bool TryParseDataRecord(string[] fields, string sourceChannel, out MeasurementRecord record)
        {
            record = null;
            if (!IsDataFrame(fields) || fields.Length < 3)
            {
                return false;
            }
            if (!uint.TryParse(fields[1], NumberStyles.Integer, CultureInfo.InvariantCulture, out uint seq))
            {
                return false;
            }
            if (!uint.TryParse(fields[2], NumberStyles.Integer, CultureInfo.InvariantCulture, out uint ts))
            {
                return false;
            }

            var samples = new List<int>();
            for (int i = 3; i < fields.Length; i++)
            {
                if (int.TryParse(fields[i], NumberStyles.Integer, CultureInfo.InvariantCulture, out int v))
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
                RawLine = string.Join(",", fields)
            };
            return true;
        }
    }
}
