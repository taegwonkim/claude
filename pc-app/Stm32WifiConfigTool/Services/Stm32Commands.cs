using System;
using System.Globalization;
using System.Threading.Tasks;
using Stm32WifiConfigTool.Models;

namespace Stm32WifiConfigTool.Services
{
    /// <summary>
    /// SET/SAVE/GET CONFIG/STATUS 프레임을 보내고 응답 프레임을 기다리는 async 헬퍼.
    /// DATA/EVENT 프레임은 비동기 텔레메트리이므로 커맨드 응답으로 취급하지 않고 건너뛴다.
    /// STX가 없는(깨진/잡음) 라인도 응답으로 취급하지 않고 무시한다.
    /// 한 번에 하나의 커맨드만 진행 중이라고 가정한다(폼에서 버튼 클릭 시 순차 호출).
    ///
    /// NOTE: 의도적으로 ConfigureAwait(false)를 쓰지 않는다. 이 클래스는 항상 WinForms 버튼
    /// 클릭 핸들러(UI 스레드, SynchronizationContext 보유)에서 호출되며, 호출자가 넘기는
    /// log 콜백(SetConfigAsync)이 컨트롤을 직접 갱신하므로 각 await 이후 UI 스레드로 되돌아와야
    /// 안전하다. 백그라운드 스레드에서 발생하는 SerialLinkService.LineReceived 이벤트는
    /// TaskCompletionSource.TrySetResult만 호출하므로 스레드 문제가 없고, 그 이후의 await
    /// 재개(continuation)가 원래 호출자의 UI 컨텍스트로 자동 복귀한다.
    /// </summary>
    public static class Stm32Commands
    {
        /// <summary>command(STX로 시작하는 프레임 문자열)를 보내고, DATA/EVENT가 아닌 첫 응답
        /// 프레임의 필드 배열을 반환한다.</summary>
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
                if (Stm32Protocol.IsDataFrame(fields) || Stm32Protocol.IsEventFrame(fields))
                {
                    return;
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

        /// <summary>GET,CONFIG를 보내고 단일 프레임 응답
        /// "CONFIG,ssid,pass_masked,server_ip,server_port,dhcp,ip,gateway,mask"을 NetConfig로 변환한다.</summary>
        public static async Task<NetConfig> GetConfigAsync(SerialLinkService link, int timeoutMs)
        {
            string[] fields = await SendAndWaitReplyAsync(link, Stm32Protocol.CmdGetConfig, timeoutMs);

            if (fields.Length == 0 || fields[0] != "CONFIG")
            {
                throw new InvalidOperationException("GET CONFIG 응답 형식 오류: " + string.Join(",", fields));
            }
            if (fields.Length < 9)
            {
                throw new InvalidOperationException("GET CONFIG 응답 필드 부족 (" + fields.Length + "/9)");
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

        /// <summary>cfg를 SET 프레임들로 순차 전송 후 SAVE까지 수행한다. 비밀번호는
        /// cfg.Password가 비어있지 않을 때만 전송한다(비어있으면 MCU에 저장된 기존 값 유지).
        /// DHCP=OFF일 때만 정적 IP/Gateway/Mask를 전송한다. 각 단계 로그는 log 콜백으로 전달.</summary>
        public static async Task SetConfigAsync(SerialLinkService link, NetConfig cfg, int timeoutMs, Action<string> log)
        {
            async Task Step(string command)
            {
                string[] reply = await SendAndWaitReplyAsync(link, command, timeoutMs);
                string cmdText = Stm32Protocol.DisplayText(command);
                string replyText = string.Join(",", reply);
                log?.Invoke(cmdText + " -> " + replyText);
                if (Stm32Protocol.IsErrorFrame(reply))
                {
                    throw new InvalidOperationException(cmdText + " 실패: " + replyText);
                }
            }

            await Step(Stm32Protocol.BuildSetSsid(cfg.Ssid));
            if (!string.IsNullOrEmpty(cfg.Password))
            {
                await Step(Stm32Protocol.BuildSetPass(cfg.Password));
            }
            await Step(Stm32Protocol.BuildSetServerIp(cfg.ServerIp));
            await Step(Stm32Protocol.BuildSetServerPort(cfg.ServerPort));
            await Step(Stm32Protocol.BuildSetDhcp(cfg.DhcpEnabled));

            if (!cfg.DhcpEnabled)
            {
                await Step(Stm32Protocol.BuildSetIp(cfg.StaticIp));
                await Step(Stm32Protocol.BuildSetGateway(cfg.Gateway));
                await Step(Stm32Protocol.BuildSetMask(cfg.Netmask));
            }

            await Step(Stm32Protocol.CmdSave);
        }

        /// <summary>STATUS를 보내고 "STATUS,&lt;WIFI UP/DOWN&gt;,&lt;TCP UP/DOWN&gt;" 응답을
        /// 사람이 읽기 쉬운 "STATUS WIFI=.. TCP=.." 문자열로 변환해 반환한다.</summary>
        public static async Task<string> GetStatusAsync(SerialLinkService link, int timeoutMs)
        {
            string[] fields = await SendAndWaitReplyAsync(link, Stm32Protocol.CmdStatus, timeoutMs);

            if (fields.Length >= 3 && fields[0] == "STATUS")
            {
                return "STATUS WIFI=" + fields[1] + " TCP=" + fields[2];
            }
            return string.Join(",", fields);
        }
    }
}
