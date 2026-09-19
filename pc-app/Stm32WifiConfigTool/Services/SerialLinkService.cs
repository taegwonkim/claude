using System;
using System.IO.Ports;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace Stm32WifiConfigTool.Services
{
    /// <summary>
    /// USB(CDC 가상 COM) 또는 UART(USART3, USB-시리얼 변환기 경유) 한 채널을 감싸는 시리얼 연결.
    /// 라인 단위("\n" 종료, docs/프로토콜_명세.md) 수신을 위해 백그라운드 스레드에서
    /// 설정된 ReadTimeout으로 폴링하며, 완성된 줄마다 <see cref="LineReceived"/>를 발생시킨다.
    /// 이벤트는 백그라운드 스레드에서 호출되므로, UI 구독자는 반드시 Invoke/BeginInvoke로
    /// UI 스레드로 마샬링해야 한다.
    /// </summary>
    public sealed class SerialLinkService : IDisposable
    {
        public LinkChannel Channel { get; }

        private SerialPort _port;
        private CancellationTokenSource _cts;
        private readonly StringBuilder _rxBuffer = new StringBuilder();
        private readonly object _rxLock = new object();
        private readonly object _writeLock = new object();

        public event Action<LinkChannel, string> LineReceived;
        public event Action<LinkChannel, bool> ConnectionChanged;
        public event Action<LinkChannel, string> ErrorOccurred;

        public bool IsConnected
        {
            get { return _port != null && _port.IsOpen; }
        }

        public string PortName { get; private set; } = string.Empty;
        public int BaudRate { get; private set; }
        public int ReadTimeoutMs { get; private set; }
        public int WriteTimeoutMs { get; private set; }

        public SerialLinkService(LinkChannel channel)
        {
            Channel = channel;
        }

        public void Connect(string portName, int baudRate, int readTimeoutMs, int writeTimeoutMs)
        {
            Disconnect();

            PortName = portName;
            BaudRate = baudRate;
            ReadTimeoutMs = readTimeoutMs;
            WriteTimeoutMs = writeTimeoutMs;

            var port = new SerialPort(portName, baudRate, Parity.None, 8, StopBits.One)
            {
                ReadTimeout = readTimeoutMs,
                WriteTimeout = writeTimeoutMs,
                Handshake = Handshake.None
            };
            port.Open();
            port.DiscardInBuffer();
            port.DiscardOutBuffer();
            _port = port;

            lock (_rxLock)
            {
                _rxBuffer.Clear();
            }

            _cts = new CancellationTokenSource();
            var token = _cts.Token;
            Task.Run(() => ReadLoop(token), token);

            ConnectionChanged?.Invoke(Channel, true);
        }

        public void Disconnect()
        {
            if (_cts != null)
            {
                _cts.Cancel();
                _cts = null;
            }

            var port = _port;
            _port = null;
            if (port != null)
            {
                try
                {
                    if (port.IsOpen)
                    {
                        port.Close();
                    }
                }
                catch (Exception)
                {
                    /* 케이블이 이미 뽑혀있는 등의 상황 - 무시하고 리소스만 정리 */
                }
                port.Dispose();

                ConnectionChanged?.Invoke(Channel, false);
            }
        }

        /// <summary>커맨드 한 줄을 "\r\n"을 붙여 전송한다.</summary>
        public void SendLine(string line)
        {
            var port = _port;
            if (port == null || !port.IsOpen)
            {
                throw new InvalidOperationException(Channel + " 포트가 연결되어 있지 않습니다.");
            }

            lock (_writeLock)
            {
                port.Write(line + "\r\n");
            }
        }

        private void ReadLoop(CancellationToken token)
        {
            var buffer = new byte[1024];

            while (!token.IsCancellationRequested)
            {
                SerialPort port = _port;
                if (port == null)
                {
                    break;
                }

                try
                {
                    int n = port.Read(buffer, 0, buffer.Length);
                    if (n > 0)
                    {
                        string chunk = Encoding.ASCII.GetString(buffer, 0, n);
                        lock (_rxLock)
                        {
                            _rxBuffer.Append(chunk);
                            ExtractLines();
                        }
                    }
                }
                catch (TimeoutException)
                {
                    /* 설정된 ReadTimeoutMs 동안 수신 없음 - 정상, 계속 폴링 */
                }
                catch (Exception ex)
                {
                    if (token.IsCancellationRequested)
                    {
                        break;
                    }
                    /* 케이블 분리 등으로 포트가 예기치 않게 닫힌 경우 */
                    ErrorOccurred?.Invoke(Channel, ex.Message);
                    ConnectionChanged?.Invoke(Channel, false);
                    break;
                }
            }
        }

        private void ExtractLines()
        {
            string content = _rxBuffer.ToString();
            int start = 0;
            int idx;

            while ((idx = content.IndexOf('\n', start)) >= 0)
            {
                string line = content.Substring(start, idx - start).TrimEnd('\r');
                start = idx + 1;
                if (line.Length > 0)
                {
                    LineReceived?.Invoke(Channel, line);
                }
            }

            _rxBuffer.Remove(0, start);
        }

        public void Dispose()
        {
            Disconnect();
        }
    }
}
