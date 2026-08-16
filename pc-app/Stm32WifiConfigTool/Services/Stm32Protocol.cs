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
    ///
    /// 메시지 분류(첫 필드 기준, 화이트리스트 방식 - 새 프레임 종류가 추가돼도 명령 응답
    /// 매칭 로직이 오작동하지 않도록):
    /// - 커맨드 응답: WIFI_R_ALL,... / WIFI_W_ALL,OK|ERR,... / MEAS_R_ALL,... / MEAS_W_ALL,OK|ERR,... / ERR,... / HELP,...
    /// - ESP32 상태(주기적 브로드캐스트 겸 STATUS 커맨드 응답): STATUS,&lt;번호&gt;
    /// - 측정값(그 외 전부, 정확히 8개 필드): &lt;DC IP&gt;,&lt;MAC&gt;,data1,...,data6 ("DATA" 태그 없음)
    /// - 비동기 이벤트: EVENT,&lt;name&gt;
    /// </summary>
    public static class Stm32Protocol
    {
        public const char Stx = '\x02';

        private static readonly HashSet<string> ReplyTags = new HashSet<string>
        {
            "WIFI_R_ALL", "WIFI_W_ALL", "MEAS_R_ALL", "MEAS_W_ALL", "ERR", "HELP"
        };

        /// <summary>WiFi(AP SSID/Password), 서버 IP/Port, DHCP/정적 IP 전체를 한 번에 조회한다.
        /// 응답: WIFI_R_ALL,ssid,pass_masked,server_ip,server_port,ON|OFF,ip,gateway,mask</summary>
        public static readonly string CmdWifiReadAll = Stx + "WIFI_R_ALL";

        /// <summary>WiFi 설정 전체를 한 프레임으로 MCU에 전달한다. 응답: WIFI_W_ALL,OK 또는 WIFI_W_ALL,ERR,&lt;reason&gt;
        /// pass가 빈 문자열이면(사용자가 "비밀번호 변경"을 체크하지 않은 경우) 필드 자체는 빈 채로
        /// 전송된다 - MCU는 이 경우 기존 저장된 비밀번호를 유지해야 한다(<see cref="Models.NetConfig.Password"/> 참고).</summary>
        public static string BuildWifiWriteAll(string ssid, string pass, string serverIp, int serverPort,
            bool dhcpOn, string ip, string gateway, string mask)
        {
            return Stx + "WIFI_W_ALL," + ssid + "," + pass + "," + serverIp + "," +
                   serverPort.ToString(CultureInfo.InvariantCulture) + "," + (dhcpOn ? "ON" : "OFF") + "," +
                   ip + "," + gateway + "," + mask;
        }

        /// <summary>측정 모듈 설정(Reference/Offset/Resistance/Interval) 전체를 한 번에 조회한다.
        /// 응답: MEAS_R_ALL,reference_mv,offset_mv,resistance_mohm,interval_sec</summary>
        public static readonly string CmdMeasReadAll = Stx + "MEAS_R_ALL";

        /// <summary>측정 모듈 설정 전체를 한 프레임으로 MCU에 전달한다. 응답: MEAS_W_ALL,OK 또는 MEAS_W_ALL,ERR,&lt;reason&gt;</summary>
        public static string BuildMeasWriteAll(double referenceMv, double offsetMv, double resistanceMOhm, double intervalSec)
        {
            return Stx + "MEAS_W_ALL," +
                   referenceMv.ToString(CultureInfo.InvariantCulture) + "," +
                   offsetMv.ToString(CultureInfo.InvariantCulture) + "," +
                   resistanceMOhm.ToString(CultureInfo.InvariantCulture) + "," +
                   intervalSec.ToString(CultureInfo.InvariantCulture);
        }

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

        /// <summary>커맨드(SET/SAVE/GET/STATUS/HELP)에 대한 직접 응답 프레임인지: OK/ERR/SAVED/CONFIG/HELP.
        /// STATUS는 주기적으로도 브로드캐스트되므로 여기 포함하지 않는다(별도 IsStatusFrame 사용).</summary>
        public static bool IsReplyFrame(string[] fields) => fields != null && fields.Length > 0 && ReplyTags.Contains(fields[0]);

        public static bool IsErrorFrame(string[] fields) => fields != null && fields.Length > 0 && fields[0] == "ERR";
        public static bool IsStatusFrame(string[] fields) => fields != null && fields.Length > 0 && fields[0] == "STATUS";
        public static bool IsEventFrame(string[] fields) => fields != null && fields.Length > 0 && fields[0] == "EVENT";

        /// <summary>로그/메시지박스 표시용: 맨 앞 STX가 있으면 제거한다(없으면 원본 그대로).</summary>
        public static string DisplayText(string s)
        {
            return (!string.IsNullOrEmpty(s) && s[0] == Stx) ? s.Substring(1) : s;
        }

        /// <summary>STATUS,&lt;번호&gt; 프레임에서 상태 번호를 꺼낸다.</summary>
        public static bool TryParseStatus(string[] fields, out int statusNumber)
        {
            statusNumber = 0;
            if (!IsStatusFrame(fields) || fields.Length < 2)
            {
                return false;
            }
            return int.TryParse(fields[1], NumberStyles.Integer, CultureInfo.InvariantCulture, out statusNumber);
        }

        /// <summary>Esp32_LinkState_t 값(0/1/2)을 사람이 읽을 수 있는 텍스트로 변환한다.</summary>
        public static string DescribeStatus(int statusNumber)
        {
            switch (statusNumber)
            {
                case 0: return "DOWN";
                case 1: return "WIFI_UP";
                case 2: return "TCP_UP";
                default: return "UNKNOWN(" + statusNumber + ")";
            }
        }

        /// <summary>측정값 프레임("&lt;DC IP&gt;,&lt;MAC&gt;,data1,...,data6", "DATA" 태그 없음)인지 확인하고
        /// 파싱한다. 커맨드 응답/STATUS가 아니고 정확히 8개 필드일 때만 측정값으로 인정한다.</summary>
        public static bool TryParseMeasurementRecord(string[] fields, string sourceChannel, out MeasurementRecord record)
        {
            record = null;
            if (fields == null || IsReplyFrame(fields) || IsStatusFrame(fields) || IsEventFrame(fields))
            {
                return false;
            }
            if (fields.Length != 8)
            {
                return false;
            }

            var samples = new List<int>();
            for (int i = 2; i < fields.Length; i++)
            {
                if (!int.TryParse(fields[i], NumberStyles.Integer, CultureInfo.InvariantCulture, out int v))
                {
                    return false; /* 데이터 필드가 숫자가 아니면 측정값 프레임이 아닌 것으로 간주 */
                }
                samples.Add(v);
            }

            record = new MeasurementRecord
            {
                ReceivedAt = DateTime.Now,
                SourceChannel = sourceChannel,
                DcIp = fields[0],
                MacAddress = fields[1],
                Samples = samples.ToArray(),
                RawLine = string.Join(",", fields)
            };
            return true;
        }
    }
}
