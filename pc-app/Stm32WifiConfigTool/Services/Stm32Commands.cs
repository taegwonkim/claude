using System;
using System.Globalization;
using System.Threading.Tasks;
using Stm32WifiConfigTool.Models;

namespace Stm32WifiConfigTool.Services
{
    /// <summary>
    /// WIFI_R_ALL/WIFI_W_ALL/MEAS_R_ALL/MEAS_W_ALL/RESET_R_ALL/RESET_W_ALL 프레임을 보내고 응답
    /// 프레임을 기다리는 async 헬퍼.
    /// 측정값/EVENT/STATUS/RESET_COUNT 프레임은 비동기 텔레메트리(브로드캐스트)이므로 일반 커맨드
    /// 응답으로 취급하지 않고 건너뛴다(<see cref="Stm32Protocol.IsBroadcastFrame"/> 참고). 그 외에는
    /// 태그가 있든("MEAS_R_ALL,...") 없든("5000,200,0,1,0"만 맨몸으로 - 실측 결과 실제 MCU가 이
    /// 방식을 씀) 진행 중인 커맨드의 응답으로 취급한다(<see cref="StripTag"/>가 두 형태를 모두
    /// 받아들인다). STX가 없는(깨진/잡음) 라인도 응답으로 취급하지 않고 무시한다.
    /// 한 번에 하나의 커맨드만 진행 중이라고 가정한다(폼에서 버튼 클릭 시 순차 호출).
    ///
    /// NOTE: 의도적으로 ConfigureAwait(false)를 쓰지 않는다. 이 클래스는 항상 WinForms 버튼
    /// 클릭 핸들러(UI 스레드, SynchronizationContext 보유)에서 호출되며, 호출자가 결과를 받아
    /// 컨트롤을 직접 갱신하므로 각 await 이후 UI 스레드로 되돌아와야 안전하다. 백그라운드
    /// 스레드에서 발생하는 SerialLinkService.LineReceived 이벤트는 TaskCompletionSource.
    /// TrySetResult만 호출하므로 스레드 문제가 없고, 그 이후의 await 재개(continuation)가
    /// 원래 호출자의 UI 컨텍스트로 자동 복귀한다.
    /// </summary>
    public static class Stm32Commands
    {
        /// <summary>command(STX로 시작하는 프레임 문자열)를 보내고, 비동기 브로드캐스트가 아닌 첫
        /// 응답 프레임의 필드 배열을 반환한다. 측정값/STATUS/EVENT/RESET_COUNT 브로드캐스트는
        /// 무시하므로, 그것들이 응답 사이사이에 섞여 도착해도 안전하다.</summary>
        public static async Task<string[]> SendAndWaitReplyAsync(SerialLinkService link, string command, int timeoutMs)
        {
            var tcs = new TaskCompletionSource<string[]>();

            void Handler(LinkChannel ch, string line)
            {
                if (ch != link.Channel)
                {
                    return;
                }
                if (!Stm32Protocol.TryParseFrame(line, out string[] fields))
                {
                    return; /* STX 없는 잡음/깨진 프레임 - 무시 */
                }
                if (Stm32Protocol.IsBroadcastFrame(fields))
                {
                    return; /* 측정값/STATUS/EVENT/RESET_COUNT 브로드캐스트 - 이 커맨드의 응답이 아님 */
                }
                tcs.TrySetResult(fields);
            }

            link.LineReceived += Handler;
            try
            {
                link.SendLine(command);
                Task completed = await Task.WhenAny(tcs.Task, Task.Delay(timeoutMs));
                if (completed != tcs.Task)
                {
                    throw new TimeoutException("응답 타임아웃: " + Stm32Protocol.DisplayText(command));
                }
                return tcs.Task.Result;
            }
            finally
            {
                link.LineReceived -= Handler;
            }
        }

        /// <summary>fields[0]이 tag와 같으면(이 저장소가 만든 firmware/firmware-no-rtos 스타일) 그
        /// 태그를 뗀 나머지 필드를 반환하고, 다르면(실제 MCU처럼 태그 없이 값만 온 경우) fields를
        /// 그대로 반환한다. 두 응답 스타일을 모두 허용하기 위한 헬퍼.</summary>
        private static string[] StripTag(string[] fields, string tag)
        {
            if (fields.Length > 0 && fields[0] == tag)
            {
                var rest = new string[fields.Length - 1];
                Array.Copy(fields, 1, rest, 0, rest.Length);
                return rest;
            }
            return fields;
        }

        /// <summary>WIFI_R_ALL을 보내고 응답(태그 있으면 "WIFI_R_ALL,..." 없으면 값만)을 NetConfig로
        /// 변환한다. 실측된 필드 순서는 "ssid,pass_masked,server_ip,server_port,dhcp"(5개, dhcp는
        /// "1"=사용/"0"=미사용)이며, 정적 IP/Gateway/Netmask는 여기 포함되지 않는다. 다만 이
        /// 저장소가 만든 firmware 스타일(8필드, dhcp 뒤에 ip,gateway,mask가 더 있는 경우)도 함께
        /// 허용한다 - 응답이 5개뿐이면 StaticIp/Gateway/Netmask는 null로 두어 호출자가 기존 값을
        /// 그대로 유지할 수 있게 한다(<see cref="Panels.WifiConfigPanel.ApplyConfigToUi"/> 참고).</summary>
        public static async Task<NetConfig> GetWifiAllAsync(SerialLinkService link, int timeoutMs)
        {
            string[] fields = await SendAndWaitReplyAsync(link, Stm32Protocol.CmdWifiReadAll, timeoutMs);
            string[] v = StripTag(fields, "WIFI_R_ALL");

            if (v.Length < 5)
            {
                throw new InvalidOperationException("WIFI_R_ALL 응답 필드 부족 (" + v.Length + "/5): " + string.Join(",", fields));
            }

            int.TryParse(v[3], NumberStyles.Integer, CultureInfo.InvariantCulture, out int port);
            bool dhcpOn = v[4] == "1" || string.Equals(v[4], "ON", StringComparison.OrdinalIgnoreCase);
            bool hasStaticIpFields = v.Length >= 8;

            return new NetConfig
            {
                Ssid = v[0],
                /* v[1] = MCU가 마스킹해서 보낸 "****" - 실제 비밀번호는 절대 돌려주지 않음 */
                ServerIp = v[2],
                ServerPort = port,
                DhcpEnabled = dhcpOn,
                StaticIp = hasStaticIpFields ? v[5] : null,
                Gateway = hasStaticIpFields ? v[6] : null,
                Netmask = hasStaticIpFields ? v[7] : null
            };
        }

        /// <summary>cfg 전체를 WIFI_W_ALL 한 프레임으로 전송한다. 응답이 "OK"(태그 있으면
        /// "WIFI_W_ALL,OK")가 아니면 실패로 간주해 예외를 던진다.</summary>
        public static async Task SetWifiAllAsync(SerialLinkService link, NetConfig cfg, int timeoutMs)
        {
            string command = Stm32Protocol.BuildWifiWriteAll(cfg.Ssid, cfg.Password, cfg.ServerIp, cfg.ServerPort,
                cfg.DhcpEnabled, cfg.StaticIp, cfg.Gateway, cfg.Netmask);
            string[] reply = await SendAndWaitReplyAsync(link, command, timeoutMs);
            string[] v = StripTag(reply, "WIFI_W_ALL");

            if (v.Length < 1 || v[0] != "OK")
            {
                throw new InvalidOperationException("WIFI_W_ALL 실패: " + string.Join(",", reply));
            }
        }

        /// <summary>MEAS_R_ALL을 보내고 응답(태그 있으면 "MEAS_R_ALL,..." 없으면 값만)
        /// "reference_mv,offset_mv,resistance_mohm,interval_sec[,...]"을 MeasurementConfig로 변환한다
        /// (실측 결과 마지막에 용도가 확인되지 않은 필드가 하나 더 붙어 올 수 있어 4개 이상이면 받는다).</summary>
        public static async Task<MeasurementConfig> GetMeasAllAsync(SerialLinkService link, int timeoutMs)
        {
            string[] fields = await SendAndWaitReplyAsync(link, Stm32Protocol.CmdMeasReadAll, timeoutMs);
            string[] v = StripTag(fields, "MEAS_R_ALL");

            if (v.Length < 4)
            {
                throw new InvalidOperationException("MEAS_R_ALL 응답 필드 부족 (" + v.Length + "/4): " + string.Join(",", fields));
            }

            double.TryParse(v[0], NumberStyles.Float, CultureInfo.InvariantCulture, out double reference);
            double.TryParse(v[1], NumberStyles.Float, CultureInfo.InvariantCulture, out double offset);
            double.TryParse(v[2], NumberStyles.Float, CultureInfo.InvariantCulture, out double resistance);
            double.TryParse(v[3], NumberStyles.Float, CultureInfo.InvariantCulture, out double interval);

            return new MeasurementConfig
            {
                ReferenceMv = reference,
                OffsetMv = offset,
                ResistanceMOhm = resistance,
                IntervalSec = interval
            };
        }

        /// <summary>cfg 전체를 MEAS_W_ALL 한 프레임으로 전송한다. 응답이 "OK"(태그 있으면
        /// "MEAS_W_ALL,OK")가 아니면 실패로 간주해 예외를 던진다.</summary>
        public static async Task SetMeasAllAsync(SerialLinkService link, MeasurementConfig cfg, int timeoutMs)
        {
            string command = Stm32Protocol.BuildMeasWriteAll(cfg.ReferenceMv, cfg.OffsetMv, cfg.ResistanceMOhm, cfg.IntervalSec);
            string[] reply = await SendAndWaitReplyAsync(link, command, timeoutMs);
            string[] v = StripTag(reply, "MEAS_W_ALL");

            if (v.Length < 1 || v[0] != "OK")
            {
                throw new InvalidOperationException("MEAS_W_ALL 실패: " + string.Join(",", reply));
            }
        }

        /// <summary>RESET_R_ALL을 보내고 응답(태그 있으면 "RESET_R_ALL,seconds" 없으면 값만
        /// "seconds")을 RtcConfig로 변환한다.</summary>
        public static async Task<RtcConfig> GetResetAllAsync(SerialLinkService link, int timeoutMs)
        {
            string[] fields = await SendAndWaitReplyAsync(link, Stm32Protocol.CmdResetReadAll, timeoutMs);
            string[] v = StripTag(fields, "RESET_R_ALL");

            if (v.Length < 1)
            {
                throw new InvalidOperationException("RESET_R_ALL 응답 필드 부족 (" + v.Length + "/1): " + string.Join(",", fields));
            }

            int.TryParse(v[0], NumberStyles.Integer, CultureInfo.InvariantCulture, out int periodSec);

            return new RtcConfig { PeriodSec = periodSec };
        }

        /// <summary>cfg.PeriodSec을 RESET_W_ALL 한 프레임으로 전송한다. 응답이 "OK"(태그 있으면
        /// "RESET_W_ALL,OK")가 아니면(ERR,MISSING_ARGS / ERR,INVALID_SECONDS 등) 실패로 간주해
        /// 예외를 던진다.</summary>
        public static async Task SetResetAllAsync(SerialLinkService link, RtcConfig cfg, int timeoutMs)
        {
            string command = Stm32Protocol.BuildResetWriteAll(cfg.PeriodSec);
            string[] reply = await SendAndWaitReplyAsync(link, command, timeoutMs);
            string[] v = StripTag(reply, "RESET_W_ALL");

            if (v.Length < 1 || v[0] != "OK")
            {
                throw new InvalidOperationException("RESET_W_ALL 실패: " + string.Join(",", reply));
            }
        }
    }
}
