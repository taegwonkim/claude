using System;
using System.Globalization;
using System.Threading.Tasks;
using Stm32WifiConfigTool.Models;

namespace Stm32WifiConfigTool.Services
{
    /// <summary>
    /// WIFI_R_ALL/WIFI_W_ALL/MEAS_R_ALL/MEAS_W_ALL 프레임을 보내고 응답 프레임을 기다리는 async 헬퍼.
    /// 측정값/EVENT/STATUS 프레임은 비동기 텔레메트리(브로드캐스트)이므로 일반 커맨드 응답으로
    /// 취급하지 않고 건너뛴다(<see cref="Stm32Protocol.IsReplyFrame"/> 화이트리스트 참고).
    /// STX가 없는(깨진/잡음) 라인도 응답으로 취급하지 않고 무시한다.
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
        /// <summary>command(STX로 시작하는 프레임 문자열)를 보내고, 커맨드 응답 프레임
        /// (OK/ERR/SAVED/CONFIG/HELP)만 골라 그 필드 배열을 반환한다. 측정값/STATUS/EVENT
        /// 브로드캐스트는 무시하므로, 그것들이 응답 사이사이에 섞여 도착해도 안전하다.</summary>
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
                if (!Stm32Protocol.IsReplyFrame(fields))
                {
                    return; /* 측정값/STATUS/EVENT 브로드캐스트 - 이 커맨드의 응답이 아님 */
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

        /// <summary>WIFI_R_ALL을 보내고 단일 프레임 응답
        /// "WIFI_R_ALL,ssid,pass_masked,server_ip,server_port,dhcp,ip,gateway,mask"을 NetConfig로 변환한다.</summary>
        public static async Task<NetConfig> GetWifiAllAsync(SerialLinkService link, int timeoutMs)
        {
            string[] fields = await SendAndWaitReplyAsync(link, Stm32Protocol.CmdWifiReadAll, timeoutMs);

            if (fields.Length == 0 || fields[0] != "WIFI_R_ALL")
            {
                throw new InvalidOperationException("WIFI_R_ALL 응답 형식 오류: " + string.Join(",", fields));
            }
            if (fields.Length < 9)
            {
                throw new InvalidOperationException("WIFI_R_ALL 응답 필드 부족 (" + fields.Length + "/9)");
            }

            int.TryParse(fields[4], NumberStyles.Integer, CultureInfo.InvariantCulture, out int port);

            return new NetConfig
            {
                Ssid = fields[1],
                /* fields[2] = MCU가 마스킹해서 보낸 "****" - 실제 비밀번호는 절대 돌려주지 않음 */
                ServerIp = fields[3],
                ServerPort = port,
                DhcpEnabled = string.Equals(fields[5], "ON", StringComparison.OrdinalIgnoreCase),
                StaticIp = fields[6],
                Gateway = fields[7],
                Netmask = fields[8]
            };
        }

        /// <summary>cfg 전체를 WIFI_W_ALL 한 프레임으로 전송한다. 응답 2번째 필드가 "OK"가
        /// 아니면 실패로 간주해 예외를 던진다.</summary>
        public static async Task SetWifiAllAsync(SerialLinkService link, NetConfig cfg, int timeoutMs)
        {
            string command = Stm32Protocol.BuildWifiWriteAll(cfg.Ssid, cfg.Password, cfg.ServerIp, cfg.ServerPort,
                cfg.DhcpEnabled, cfg.StaticIp, cfg.Gateway, cfg.Netmask);
            string[] reply = await SendAndWaitReplyAsync(link, command, timeoutMs);

            if (reply.Length < 2 || reply[0] != "WIFI_W_ALL" || reply[1] != "OK")
            {
                throw new InvalidOperationException("WIFI_W_ALL 실패: " + string.Join(",", reply));
            }
        }

        /// <summary>MEAS_R_ALL을 보내고 단일 프레임 응답
        /// "MEAS_R_ALL,reference_mv,offset_mv,resistance_mohm,interval_sec"을 MeasurementConfig로 변환한다.</summary>
        public static async Task<MeasurementConfig> GetMeasAllAsync(SerialLinkService link, int timeoutMs)
        {
            string[] fields = await SendAndWaitReplyAsync(link, Stm32Protocol.CmdMeasReadAll, timeoutMs);

            if (fields.Length == 0 || fields[0] != "MEAS_R_ALL")
            {
                throw new InvalidOperationException("MEAS_R_ALL 응답 형식 오류: " + string.Join(",", fields));
            }
            if (fields.Length < 5)
            {
                throw new InvalidOperationException("MEAS_R_ALL 응답 필드 부족 (" + fields.Length + "/5)");
            }

            double.TryParse(fields[1], NumberStyles.Float, CultureInfo.InvariantCulture, out double reference);
            double.TryParse(fields[2], NumberStyles.Float, CultureInfo.InvariantCulture, out double offset);
            double.TryParse(fields[3], NumberStyles.Float, CultureInfo.InvariantCulture, out double resistance);
            double.TryParse(fields[4], NumberStyles.Float, CultureInfo.InvariantCulture, out double interval);

            return new MeasurementConfig
            {
                ReferenceMv = reference,
                OffsetMv = offset,
                ResistanceMOhm = resistance,
                IntervalSec = interval
            };
        }

        /// <summary>cfg 전체를 MEAS_W_ALL 한 프레임으로 전송한다. 응답 2번째 필드가 "OK"가
        /// 아니면 실패로 간주해 예외를 던진다.</summary>
        public static async Task SetMeasAllAsync(SerialLinkService link, MeasurementConfig cfg, int timeoutMs)
        {
            string command = Stm32Protocol.BuildMeasWriteAll(cfg.ReferenceMv, cfg.OffsetMv, cfg.ResistanceMOhm, cfg.IntervalSec);
            string[] reply = await SendAndWaitReplyAsync(link, command, timeoutMs);

            if (reply.Length < 2 || reply[0] != "MEAS_W_ALL" || reply[1] != "OK")
            {
                throw new InvalidOperationException("MEAS_W_ALL 실패: " + string.Join(",", reply));
            }
        }
    }
}
