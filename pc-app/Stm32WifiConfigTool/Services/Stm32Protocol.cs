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
    /// 메시지 분류: 실측 결과 실제 MCU는 커맨드 응답에 "MEAS_R_ALL,..." 같은 태그를 붙이지
    /// 않고 값만 맨몸으로 돌려준다(예: 쓰기 응답은 그냥 "OK", 읽기 응답은 그냥 "5000,200,0,1,0").
    /// 이 저장소 문서(docs/프로토콜_명세.md)가 가정한 "태그 있는 응답"과 다르므로, 첫 필드가
    /// 커맨드명과 같은지로 응답을 식별하는 화이트리스트 방식은 쓸 수 없다. 대신 "확실하게 비동기
    /// 브로드캐스트로만 쓰이는 태그가 아니면 전부 응답 후보로 취급"하는 블랙리스트(제외) 방식을
    /// 쓴다(<see cref="IsBroadcastFrame"/>) — 한 번에 하나의 커맨드-응답만 진행한다는 가정(여러
    /// 패널이 동시에 커맨드를 보내지 않음) 하에, 대기 중 도착한 비-브로드캐스트 첫 줄을 그 커맨드의
    /// 응답으로 본다. 응답이 태그 있는 형태("MEAS_R_ALL,...")로 오는 경우도(이 저장소가 만든
    /// firmware/firmware-no-rtos처럼) 여전히 정상 인식되도록, 각 Get/Set 헬퍼(Stm32Commands.cs)는
    /// 태그가 있으면 건너뛰고 없으면 그대로 쓰는 방식으로 둘 다 허용한다.
    /// - 비동기 브로드캐스트(항상 응답 아님으로 제외): STATUS,&lt;번호&gt; / EVENT,&lt;name&gt; /
    ///   RESET_COUNT,&lt;count&gt; / 측정값(DC_&lt;dc_ip&gt;,&lt;mac&gt;,data1,...,dataN — 첫 필드가
    ///   리터럴 "DC_" 접두어로 시작하는 것으로 식별, 샘플 개수 N은 고정 아님. 실측 결과 6개가
    ///   아니라 12개까지 관측되어 필드 개수가 아니라 이 접두어로만 판별한다)
    /// - 그 외 전부: 커맨드 응답 후보(태그가 있든 없든)
    /// </summary>
    public static class Stm32Protocol
    {
        public const char Stx = '\x02';

        private const string MeasurementTagPrefix = "DC_";

        /// <summary>WiFi(AP SSID/Password), 서버 IP/Port, DHCP 사용유무(+정적 IP)를 한 번에 조회한다.
        /// 응답: WIFI_R_ALL,ssid,pass_masked,server_ip,server_port,dhcp[,ip,gateway,mask] — dhcp는
        /// 실측 결과 "1"=사용/"0"=미사용(문서가 가정했던 "ON"/"OFF" 텍스트가 아니다). 정적 IP/
        /// Gateway/Netmask 3개 필드는 **dhcp="0"(미사용)일 때만** dhcp 필드 뒤에 이어서 온다 —
        /// dhcp="1"(사용)이면 이 3개 필드 자체가 없다(<see cref="Stm32Commands.GetWifiAllAsync"/> 참고).</summary>
        public static readonly string CmdWifiReadAll = Stx + "WIFI_R_ALL";

        /// <summary>WiFi 설정 전체를 한 프레임으로 MCU에 전달한다. 응답: WIFI_W_ALL,OK 또는 WIFI_W_ALL,ERR,&lt;reason&gt;
        /// pass가 빈 문자열이면(사용자가 "비밀번호 변경"을 체크하지 않은 경우) 필드 자체는 빈 채로
        /// 전송된다 - MCU는 이 경우 기존 저장된 비밀번호를 유지해야 한다(<see cref="Models.NetConfig.Password"/> 참고).
        /// dhcpOn은 실측된 형식대로 "1"/"0"으로 인코딩하며(문서가 가정했던 "ON"/"OFF"가 아님),
        /// ip/gateway/mask 3개 필드는 dhcpOn이 false(정적 IP 사용)일 때만 뒤에 덧붙인다 — dhcpOn이
        /// true면 이 3개 필드 자체를 보내지 않는다(응답과 대칭, <see cref="CmdWifiReadAll"/> 참고).</summary>
        public static string BuildWifiWriteAll(string ssid, string pass, string serverIp, int serverPort,
            bool dhcpOn, string ip, string gateway, string mask)
        {
            string frame = Stx + "WIFI_W_ALL," + ssid + "," + pass + "," + serverIp + "," +
                   serverPort.ToString(CultureInfo.InvariantCulture) + "," + (dhcpOn ? "1" : "0");
            if (!dhcpOn)
            {
                frame += "," + ip + "," + gateway + "," + mask;
            }
            return frame;
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

        /// <summary>RTC Wakeup Timer 리셋 주기(초)를 한 번에 조회한다.
        /// 응답: RESET_R_ALL,seconds</summary>
        public static readonly string CmdResetReadAll = Stx + "RESET_R_ALL";

        /// <summary>RTC Wakeup Timer 리셋 주기(초)를 MCU에 전달한다. 응답: RESET_W_ALL,OK 또는
        /// RESET_W_ALL,ERR,&lt;reason&gt; (MISSING_ARGS/INVALID_SECONDS)</summary>
        public static string BuildResetWriteAll(int periodSec)
        {
            return Stx + "RESET_W_ALL," + periodSec.ToString(CultureInfo.InvariantCulture);
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

        public static bool IsErrorFrame(string[] fields) => fields != null && fields.Length > 0 && fields[0] == "ERR";
        public static bool IsStatusFrame(string[] fields) => fields != null && fields.Length > 0 && fields[0] == "STATUS";
        public static bool IsEventFrame(string[] fields) => fields != null && fields.Length > 0 && fields[0] == "EVENT";
        public static bool IsResetCountFrame(string[] fields) => fields != null && fields.Length > 0 && fields[0] == "RESET_COUNT";
        public static bool IsMeasurementFrame(string[] fields) =>
            fields != null && fields.Length >= 3 && fields[0].StartsWith(MeasurementTagPrefix, StringComparison.Ordinal);

        /// <summary>비동기 브로드캐스트 프레임인지(=커맨드 응답 후보에서 제외해야 하는지). 클래스
        /// 주석 참고 - 이것이 아닌 나머지 전부는 진행 중인 커맨드의 응답 후보로 취급한다.</summary>
        public static bool IsBroadcastFrame(string[] fields) =>
            IsStatusFrame(fields) || IsEventFrame(fields) || IsResetCountFrame(fields) || IsMeasurementFrame(fields);

        /// <summary>로그/메시지박스 표시용: 맨 앞 STX가 있으면 제거한다(없으면 원본 그대로).</summary>
        public static string DisplayText(string s)
        {
            return (!string.IsNullOrEmpty(s) && s[0] == Stx) ? s.Substring(1) : s;
        }

        /// <summary>STX 유무와 관계없이(<see cref="DisplayText"/>로 이미 STX를 뗀) 원본 텍스트에서
        /// STATUS 상태 번호를 꺼낸다. 이 저장소 firmware/docs가 가정한 "STATUS,&lt;번호&gt;"(콤마)
        /// 뿐 아니라, 실측된 실제 MCU의 "STATUS:&lt;번호&gt;"(콜론) 형식도 함께 받아들인다.</summary>
        public static bool TryParseStatusText(string text, out int statusNumber)
        {
            statusNumber = 0;
            const string prefix = "STATUS";
            if (string.IsNullOrEmpty(text) || text.Length <= prefix.Length ||
                !text.StartsWith(prefix, StringComparison.Ordinal))
            {
                return false;
            }
            char sep = text[prefix.Length];
            if (sep != ':' && sep != ',')
            {
                return false;
            }
            return int.TryParse(text.Substring(prefix.Length + 1), NumberStyles.Integer, CultureInfo.InvariantCulture, out statusNumber);
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

        /// <summary>측정값 프레임("DC_&lt;dc_ip&gt;,&lt;mac&gt;,data1,...,dataN")인지 확인하고 파싱한다.
        /// 첫 필드가 "DC_" 접두어로 시작하는지로 식별한다(실측 결과 샘플 개수가 고정이 아니어서
        /// 필드 개수로는 판별할 수 없음 - 위 클래스 주석 참고). 접두어 확인 후에는 MAC을 포함해
        /// 최소 1개 이상의 샘플 필드가 있어야 하고, 샘플 필드는 모두 정수로 파싱되어야 한다.</summary>
        public static bool TryParseMeasurementRecord(string[] fields, string sourceChannel, out MeasurementRecord record)
        {
            record = null;
            if (!IsMeasurementFrame(fields))
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
                DcIp = fields[0].Substring(MeasurementTagPrefix.Length),
                MacAddress = fields[1],
                Samples = samples.ToArray(),
                RawLine = string.Join(",", fields)
            };
            return true;
        }
    }
}
