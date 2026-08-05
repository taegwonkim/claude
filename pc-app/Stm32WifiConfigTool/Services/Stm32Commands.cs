using System;
using System.Collections.Generic;
using System.Globalization;
using System.Threading.Tasks;
using Stm32WifiConfigTool.Models;

namespace Stm32WifiConfigTool.Services
{
    /// <summary>
    /// SET/SAVE/GET CONFIG/STATUS 커맨드를 보내고 응답 라인을 기다리는 async 헬퍼.
    /// DATA/EVENT 라인은 비동기 텔레메트리이므로 커맨드 응답으로 취급하지 않고 건너뛴다.
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
        /// <summary>command를 보내고, DATA/EVENT가 아닌 첫 응답 줄을 반환한다.</summary>
        public static async Task<string> SendAndWaitReplyAsync(SerialLinkService link, string command, int timeoutMs)
        {
            var tcs = new TaskCompletionSource<string>();

            void Handler(LinkChannel ch, string line)
            {
                if (ch != link.Channel)
                {
                    return;
                }
                if (Stm32Protocol.IsDataLine(line) || Stm32Protocol.IsEventLine(line))
                {
                    return;
                }
                tcs.TrySetResult(line);
            }

            link.LineReceived += Handler;
            try
            {
                link.SendLine(command);
                Task completed = await Task.WhenAny(tcs.Task, Task.Delay(timeoutMs));
                if (completed != tcs.Task)
                {
                    throw new TimeoutException("응답 타임아웃: " + command);
                }
                return tcs.Task.Result;
            }
            finally
            {
                link.LineReceived -= Handler;
            }
        }

        /// <summary>GET CONFIG를 보내고 8개 키(SSID/PASS/SERVER_IP/SERVER_PORT/DHCP/IP/GATEWAY/MASK)가
        /// 모두 도착할 때까지 기다려 NetConfig로 조립한다.</summary>
        public static async Task<NetConfig> GetConfigAsync(SerialLinkService link, int timeoutMs)
        {
            var cfg = new NetConfig();
            var receivedKeys = new HashSet<string>();
            var tcs = new TaskCompletionSource<bool>();
            string[] expectedKeys = { "SSID", "PASS", "SERVER_IP", "SERVER_PORT", "DHCP", "IP", "GATEWAY", "MASK" };

            void Handler(LinkChannel ch, string line)
            {
                if (ch != link.Channel)
                {
                    return;
                }
                if (!Stm32Protocol.TryParseKeyValue(line, out string key, out string value))
                {
                    return;
                }

                switch (key)
                {
                    case "SSID":
                        cfg.Ssid = value;
                        break;
                    case "PASS":
                        break; /* 마스킹된 값("****")이므로 무시 */
                    case "SERVER_IP":
                        cfg.ServerIp = value;
                        break;
                    case "SERVER_PORT":
                        if (int.TryParse(value, NumberStyles.Integer, CultureInfo.InvariantCulture, out int port))
                        {
                            cfg.ServerPort = port;
                        }
                        break;
                    case "DHCP":
                        cfg.DhcpEnabled = string.Equals(value, "ON", StringComparison.OrdinalIgnoreCase);
                        break;
                    case "IP":
                        cfg.StaticIp = value;
                        break;
                    case "GATEWAY":
                        cfg.Gateway = value;
                        break;
                    case "MASK":
                        cfg.Netmask = value;
                        break;
                    default:
                        return; /* 알 수 없는 키는 응답 카운트에 포함하지 않음 */
                }

                receivedKeys.Add(key);
                if (receivedKeys.Count >= expectedKeys.Length)
                {
                    tcs.TrySetResult(true);
                }
            }

            link.LineReceived += Handler;
            try
            {
                link.SendLine(Stm32Protocol.CmdGetConfig);
                Task completed = await Task.WhenAny(tcs.Task, Task.Delay(timeoutMs));
                if (completed != tcs.Task)
                {
                    throw new TimeoutException("GET CONFIG 응답 타임아웃 (받은 필드: " + receivedKeys.Count + "/" + expectedKeys.Length + ")");
                }
                return cfg;
            }
            finally
            {
                link.LineReceived -= Handler;
            }
        }

        /// <summary>cfg를 SET 커맨드들로 순차 전송 후 SAVE까지 수행한다. 비밀번호는
        /// cfg.Password가 비어있지 않을 때만 전송한다(비어있으면 MCU에 저장된 기존 값 유지).
        /// DHCP=OFF일 때만 정적 IP/Gateway/Mask를 전송한다. 각 단계 로그는 log 콜백으로 전달.</summary>
        public static async Task SetConfigAsync(SerialLinkService link, NetConfig cfg, int timeoutMs, Action<string> log)
        {
            async Task Step(string command)
            {
                string reply = await SendAndWaitReplyAsync(link, command, timeoutMs);
                log?.Invoke(command + " -> " + reply);
                if (Stm32Protocol.IsErrorLine(reply))
                {
                    throw new InvalidOperationException(command + " 실패: " + reply);
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

        /// <summary>STATUS 커맨드를 보내고 "STATUS WIFI=.. TCP=.." 원문을 그대로 반환한다.</summary>
        public static Task<string> GetStatusAsync(SerialLinkService link, int timeoutMs)
        {
            return SendAndWaitReplyAsync(link, Stm32Protocol.CmdStatus, timeoutMs);
        }
    }
}
