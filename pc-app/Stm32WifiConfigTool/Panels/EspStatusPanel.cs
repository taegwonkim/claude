using System;
using System.Drawing;
using System.Windows.Forms;
using Stm32WifiConfigTool.Models;
using Stm32WifiConfigTool.Services;

namespace Stm32WifiConfigTool.Panels
{
    /// <summary>
    /// ESP32 상태("STATUS,&lt;번호&gt;" 프레임) 표시 패널. MCU는 측정값 전송 사이사이에
    /// 이 프레임을 주기적으로 브로드캐스트한다(docs/프로토콜_명세.md §1). 측정값 프레임과는
    /// 별도로 구분해서 여기 표시한다. USB/UART 채널을 선택해 어느 쪽(또는 둘 다)을 표시할지
    /// 고를 수 있다. UI 레이아웃은 <c>EspStatusPanel.Designer.cs</c>에 있으며 Visual Studio
    /// 디자이너로 편집 가능하다. 매개변수 없는 생성자는 디자이너 전용이며, 실제 사용 시에는
    /// 생성 직후 <see cref="Initialize"/>를 호출해 런타임 의존성(ConnectionManager, AppSettings)을
    /// 연결해야 한다. MainForm에 다른 패널들과 함께 한 창에 도킹되어 표시된다.
    /// </summary>
    public partial class EspStatusPanel : UserControl
    {
        private const int MaxLogLines = 2000;

        private ConnectionManager _conn;
        private AppSettings _settings;
        private int _logLineCount;

        public EspStatusPanel()
        {
            InitializeComponent();
        }

        /// <summary>디자이너가 만든 컨트롤에 실제 동작을 연결한다. MainForm이 생성 직후 1회 호출.</summary>
        public void Initialize(ConnectionManager conn, AppSettings settings)
        {
            _conn = conn;
            _settings = settings;

            _showUsb.Checked = settings.EspStatusDisplayChannel == "Usb";
            _showUart.Checked = settings.EspStatusDisplayChannel == "Uart";
            _showBoth.Checked = settings.EspStatusDisplayChannel != "Usb" && settings.EspStatusDisplayChannel != "Uart";

            _conn.Usb.LineReceived += OnLineReceived;
            _conn.Uart.LineReceived += OnLineReceived;
        }

        private void ShowUsb_CheckedChanged(object sender, EventArgs e)
        {
            if (_showUsb.Checked && _settings != null)
            {
                _settings.EspStatusDisplayChannel = "Usb";
            }
        }

        private void ShowUart_CheckedChanged(object sender, EventArgs e)
        {
            if (_showUart.Checked && _settings != null)
            {
                _settings.EspStatusDisplayChannel = "Uart";
            }
        }

        private void ShowBoth_CheckedChanged(object sender, EventArgs e)
        {
            if (_showBoth.Checked && _settings != null)
            {
                _settings.EspStatusDisplayChannel = "Both";
            }
        }

        private bool IsChannelSelected(LinkChannel channel)
        {
            if (_showBoth.Checked)
            {
                return true;
            }
            return (channel == LinkChannel.Usb && _showUsb.Checked) || (channel == LinkChannel.Uart && _showUart.Checked);
        }

        private static string ChannelLabel(LinkChannel channel) => channel == LinkChannel.Usb ? "USB" : "UART";

        private static Color ColorForStatus(int statusNumber)
        {
            switch (statusNumber)
            {
                case 0: return Color.Firebrick;
                case 1: return Color.DarkOrange;
                case 2: return Color.SeaGreen;
                default: return Color.Gray;
            }
        }

        // SerialLinkService.LineReceived는 백그라운드 읽기 스레드에서 호출되므로 반드시 UI 스레드로 마샬링한다.
        private void OnLineReceived(LinkChannel channel, string line)
        {
            if (IsDisposed || !IsHandleCreated)
            {
                return;
            }

            try
            {
                BeginInvoke(new Action(() => HandleLineOnUiThread(channel, line)));
            }
            catch (ObjectDisposedException)
            {
                /* 폼이 닫히는 중 - 무시 */
            }
        }

        private void HandleLineOnUiThread(LinkChannel channel, string line)
        {
            if (!IsChannelSelected(channel))
            {
                return;
            }

            if (!Stm32Protocol.TryParseFrame(line, out string[] fields))
            {
                return; /* STX 없는 잡음/깨진 프레임 - 무시 */
            }

            if (!Stm32Protocol.TryParseStatus(fields, out int statusNumber))
            {
                return; /* STATUS 프레임이 아님 */
            }

            string text = Stm32Protocol.DescribeStatus(statusNumber);
            DateTime now = DateTime.Now;

            _currentStatusLabel.Text = text + " (" + statusNumber + ")";
            _currentStatusLabel.ForeColor = ColorForStatus(statusNumber);
            _lastUpdateLabel.Text = "마지막 수신: " + now.ToString("HH:mm:ss.fff") + "  [" + ChannelLabel(channel) + "]";

            AppendLog(now.ToString("HH:mm:ss.fff") + "  [" + ChannelLabel(channel) + "] STATUS," + statusNumber + " (" + text + ")");
        }

        private void AppendLog(string text)
        {
            _logBox.AppendText(text + Environment.NewLine);
            _logLineCount++;

            if (_logLineCount > MaxLogLines)
            {
                /* 오래된 줄부터 잘라내 메모리를 보호한다 */
                int cut = _logBox.Text.IndexOf('\n');
                if (cut >= 0)
                {
                    _logBox.Text = _logBox.Text.Substring(cut + 1);
                    _logLineCount--;
                }
            }
        }

        private void ClearButton_Click(object sender, EventArgs e)
        {
            _logBox.Clear();
            _logLineCount = 0;
            _currentStatusLabel.Text = "-";
            _currentStatusLabel.ForeColor = Color.Gray;
            _lastUpdateLabel.Text = "수신 대기 중...";
        }

        protected override void Dispose(bool disposing)
        {
            if (disposing)
            {
                if (_conn != null)
                {
                    _conn.Usb.LineReceived -= OnLineReceived;
                    _conn.Uart.LineReceived -= OnLineReceived;
                }
                components?.Dispose();
            }
            base.Dispose(disposing);
        }
    }
}
