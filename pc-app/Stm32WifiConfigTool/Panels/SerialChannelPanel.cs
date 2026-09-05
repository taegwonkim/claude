using System;
using System.IO.Ports;
using System.Windows.Forms;
using Stm32WifiConfigTool.Models;
using Stm32WifiConfigTool.Services;

namespace Stm32WifiConfigTool.Panels
{
    /// <summary>
    /// 채널 하나(USB 또는 UART)의 포트/보레이트/타임아웃/연결 UI. PortSettingsPanel에서 2개(USB/UART) 사용.
    /// UI 레이아웃은 <c>SerialChannelPanel.Designer.cs</c>에 있으며 Visual Studio 디자이너로 편집 가능하다.
    /// 매개변수 없는 생성자는 디자이너 전용이며, 실제 사용 시에는 생성 직후 <see cref="Initialize"/>를
    /// 호출해 런타임 의존성(SerialLinkService, ChannelSettings)을 연결해야 한다.
    /// </summary>
    public partial class SerialChannelPanel : UserControl
    {
        private static readonly int[] BaudRates = { 9600, 19200, 38400, 57600, 115200, 230400 };

        private SerialLinkService _link;
        private ChannelSettings _settings;

        public SerialChannelPanel()
        {
            InitializeComponent();
        }

        /// <summary>디자이너가 만든 컨트롤에 실제 동작을 연결한다. PortSettingsPanel이 생성 직후 1회 호출.</summary>
        public void Initialize(string title, SerialLinkService link, ChannelSettings settings)
        {
            _link = link;
            _settings = settings;
            _groupBox.Text = title;

            _baudCombo.Items.Clear();
            _baudCombo.Items.AddRange(Array.ConvertAll(BaudRates, b => (object)b));
            _baudCombo.SelectedItem = Array.IndexOf(BaudRates, settings.BaudRate) >= 0 ? settings.BaudRate : 115200;

            _readTimeout.Value = Clamp(settings.ReadTimeoutMs, _readTimeout.Minimum, _readTimeout.Maximum);
            _writeTimeout.Value = Clamp(settings.WriteTimeoutMs, _writeTimeout.Minimum, _writeTimeout.Maximum);

            _link.ConnectionChanged += Link_ConnectionChanged;
            RefreshPorts();

            // 값이 바뀔 때마다 전달받은 ChannelSettings에 실시간 반영 (저장은 앱 종료 시 MainForm이 일괄 수행)
            _portCombo.SelectedIndexChanged += PortCombo_SelectedIndexChanged;
            _baudCombo.SelectedIndexChanged += BaudCombo_SelectedIndexChanged;
            _readTimeout.ValueChanged += ReadTimeout_ValueChanged;
            _writeTimeout.ValueChanged += WriteTimeout_ValueChanged;
        }

        private static decimal Clamp(int value, decimal min, decimal max)
        {
            if (value < min) return min;
            if (value > max) return max;
            return value;
        }

        private void PortCombo_SelectedIndexChanged(object sender, EventArgs e)
        {
            if (_portCombo.SelectedItem is string port)
            {
                _settings.PortName = port;
            }
        }

        private void BaudCombo_SelectedIndexChanged(object sender, EventArgs e)
        {
            if (_baudCombo.SelectedItem is int baud)
            {
                _settings.BaudRate = baud;
            }
        }

        private void ReadTimeout_ValueChanged(object sender, EventArgs e)
        {
            _settings.ReadTimeoutMs = (int)_readTimeout.Value;
        }

        private void WriteTimeout_ValueChanged(object sender, EventArgs e)
        {
            _settings.WriteTimeoutMs = (int)_writeTimeout.Value;
        }

        private void RefreshButton_Click(object sender, EventArgs e)
        {
            RefreshPorts();
        }

        private void RefreshPorts()
        {
            string current = _portCombo.SelectedItem as string ?? _settings?.PortName;
            _portCombo.Items.Clear();
            _portCombo.Items.AddRange(SerialPort.GetPortNames());
            if (!string.IsNullOrEmpty(current) && _portCombo.Items.Contains(current))
            {
                _portCombo.SelectedItem = current;
            }
            else if (_portCombo.Items.Count > 0)
            {
                _portCombo.SelectedIndex = 0;
            }
        }

        private void ConnectButton_Click(object sender, EventArgs e)
        {
            if (_link.IsConnected)
            {
                _link.Disconnect();
                return;
            }

            if (_portCombo.SelectedItem == null)
            {
                MessageBox.Show(this, "COM 포트를 선택하세요.", "포트 설정", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return;
            }

            try
            {
                _link.Connect(
                    (string)_portCombo.SelectedItem,
                    (int)_baudCombo.SelectedItem,
                    (int)_readTimeout.Value,
                    (int)_writeTimeout.Value);
            }
            catch (Exception ex)
            {
                MessageBox.Show(this, "연결 실패: " + ex.Message, "포트 설정", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        private void Link_ConnectionChanged(LinkChannel channel, bool connected)
        {
            if (IsDisposed)
            {
                return;
            }
            BeginInvoke(new Action(() =>
            {
                if (connected)
                {
                    _statusLabel.Text = "연결됨 (" + _link.PortName + ", " + _link.BaudRate + "bps)";
                    _statusLabel.ForeColor = System.Drawing.Color.SeaGreen;
                    _connectButton.Text = "연결 해제";
                    _portCombo.Enabled = false;
                    _baudCombo.Enabled = false;
                    _readTimeout.Enabled = false;
                    _writeTimeout.Enabled = false;
                }
                else
                {
                    _statusLabel.Text = "연결 안됨";
                    _statusLabel.ForeColor = System.Drawing.Color.Firebrick;
                    _connectButton.Text = "연결";
                    _portCombo.Enabled = true;
                    _baudCombo.Enabled = true;
                    _readTimeout.Enabled = true;
                    _writeTimeout.Enabled = true;
                }
            }));
        }

        protected override void Dispose(bool disposing)
        {
            if (disposing)
            {
                if (_link != null)
                {
                    _link.ConnectionChanged -= Link_ConnectionChanged;
                }
                components?.Dispose();
            }
            base.Dispose(disposing);
        }
    }
}
