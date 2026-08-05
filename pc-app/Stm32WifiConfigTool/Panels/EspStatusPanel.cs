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
    /// 고를 수 있다. MainForm에 다른 패널들과 함께 한 창에 도킹되어 표시된다.
    /// </summary>
    public class EspStatusPanel : UserControl
    {
        private const int MaxLogLines = 2000;

        private readonly RadioButton _showUsb;
        private readonly RadioButton _showUart;
        private readonly RadioButton _showBoth;
        private readonly Label _currentStatusLabel;
        private readonly Label _lastUpdateLabel;
        private readonly TextBox _logBox;
        private readonly ConnectionManager _conn;

        private int _logLineCount;

        public EspStatusPanel(ConnectionManager conn, AppSettings settings)
        {
            _conn = conn;

            var root = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 1, RowCount = 5, Padding = new Padding(6) };
            root.RowStyles.Add(new RowStyle(SizeType.AutoSize)); // 0: 채널 선택
            root.RowStyles.Add(new RowStyle(SizeType.AutoSize)); // 1: 현재 상태
            root.RowStyles.Add(new RowStyle(SizeType.AutoSize)); // 2: 지우기 버튼
            root.RowStyles.Add(new RowStyle(SizeType.AutoSize)); // 3: "수신 이력" 라벨
            root.RowStyles.Add(new RowStyle(SizeType.Percent, 100)); // 4: 로그 박스

            // 채널 선택
            var channelGroup = new GroupBox { Text = "표시 채널", Dock = DockStyle.Top, Height = 50 };
            _showUsb = new RadioButton { Text = "USB", Left = 10, Top = 20, AutoSize = true, Checked = settings.EspStatusDisplayChannel == "Usb" };
            _showUart = new RadioButton { Text = "UART", Left = 70, Top = 20, AutoSize = true, Checked = settings.EspStatusDisplayChannel == "Uart" };
            _showBoth = new RadioButton { Text = "둘 다", Left = 140, Top = 20, AutoSize = true, Checked = settings.EspStatusDisplayChannel != "Usb" && settings.EspStatusDisplayChannel != "Uart" };
            _showUsb.CheckedChanged += (s, e) => { if (_showUsb.Checked) settings.EspStatusDisplayChannel = "Usb"; };
            _showUart.CheckedChanged += (s, e) => { if (_showUart.Checked) settings.EspStatusDisplayChannel = "Uart"; };
            _showBoth.CheckedChanged += (s, e) => { if (_showBoth.Checked) settings.EspStatusDisplayChannel = "Both"; };
            channelGroup.Controls.Add(_showUsb);
            channelGroup.Controls.Add(_showUart);
            channelGroup.Controls.Add(_showBoth);
            root.Controls.Add(channelGroup, 0, 0);

            // 현재 상태 큰 표시
            var currentGroup = new GroupBox { Text = "현재 ESP32 상태", Dock = DockStyle.Top, Height = 90 };
            _currentStatusLabel = new Label
            {
                Text = "-",
                Left = 10,
                Top = 20,
                AutoSize = true,
                Font = new Font(Font.FontFamily, 16f, FontStyle.Bold),
                ForeColor = Color.Gray
            };
            _lastUpdateLabel = new Label { Text = "수신 대기 중...", Left = 10, Top = 58, AutoSize = true, ForeColor = Color.DimGray };
            currentGroup.Controls.Add(_currentStatusLabel);
            currentGroup.Controls.Add(_lastUpdateLabel);
            root.Controls.Add(currentGroup, 0, 1);

            var clearButton = new Button { Text = "지우기", AutoSize = true, Dock = DockStyle.Top, Margin = new Padding(0, 4, 0, 4) };
            clearButton.Click += ClearButton_Click;
            root.Controls.Add(clearButton, 0, 2);

            var logLabel = new Label { Text = "수신 이력", Dock = DockStyle.Top, AutoSize = true, Padding = new Padding(0, 4, 0, 2) };
            root.Controls.Add(logLabel, 0, 3);

            _logBox = new TextBox
            {
                Dock = DockStyle.Fill,
                Multiline = true,
                ReadOnly = true,
                ScrollBars = ScrollBars.Vertical,
                Font = new Font(FontFamily.GenericMonospace, 8.5f)
            };
            root.Controls.Add(_logBox, 0, 4);

            Controls.Add(root);

            _conn.Usb.LineReceived += OnLineReceived;
            _conn.Uart.LineReceived += OnLineReceived;
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
                _conn.Usb.LineReceived -= OnLineReceived;
                _conn.Uart.LineReceived -= OnLineReceived;
            }
            base.Dispose(disposing);
        }
    }
}
