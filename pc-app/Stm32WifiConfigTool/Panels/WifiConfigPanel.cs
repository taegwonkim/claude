using System;
using System.Windows.Forms;
using Stm32WifiConfigTool.Models;
using Stm32WifiConfigTool.Services;

namespace Stm32WifiConfigTool.Panels
{
    /// <summary>
    /// WiFi(AP SSID/Password), 서버 IP/Port, DHCP/정적 IP 설정 패널.
    /// "현재값 읽기"로 MCU의 GET CONFIG 응답을 화면에 채우고, "저장"으로 SET+SAVE를 순차 전송한다.
    /// UI 레이아웃은 <c>WifiConfigPanel.Designer.cs</c>에 있으며 Visual Studio 디자이너로 편집 가능하다.
    /// 매개변수 없는 생성자는 디자이너 전용이며, 실제 사용 시에는 생성 직후 <see cref="Initialize"/>를
    /// 호출해 런타임 의존성(ConnectionManager, AppSettings)을 연결해야 한다.
    /// MainForm에 다른 패널들과 함께 한 창에 도킹되어 표시된다.
    /// </summary>
    public partial class WifiConfigPanel : UserControl
    {
        private ConnectionManager _conn;
        private AppSettings _settings;

        public WifiConfigPanel()
        {
            InitializeComponent();
        }

        /// <summary>디자이너가 만든 컨트롤에 실제 동작을 연결한다. MainForm이 생성 직후 1회 호출.</summary>
        public void Initialize(ConnectionManager conn, AppSettings settings)
        {
            _conn = conn;
            _settings = settings;

            bool useUart = settings.WifiCommandChannel == "Uart";
            _channelUsb.Checked = !useUart;
            _channelUart.Checked = useUart;

            _cmdTimeoutBox.Value = ClampDecimal(settings.WifiCommandTimeoutMs, _cmdTimeoutBox.Minimum, _cmdTimeoutBox.Maximum);
        }

        private static decimal ClampDecimal(int value, decimal min, decimal max)
        {
            if (value < min) return min;
            if (value > max) return max;
            return value;
        }

        private SerialLinkService SelectedLink => _channelUsb.Checked ? _conn.Usb : _conn.Uart;

        private void Log(string text)
        {
            _logBox.AppendText(DateTime.Now.ToString("HH:mm:ss.fff") + "  " + text + Environment.NewLine);
        }

        private bool EnsureConnected()
        {
            if (SelectedLink.IsConnected)
            {
                return true;
            }
            MessageBox.Show(this, "선택한 채널(" + (_channelUsb.Checked ? "USB" : "UART") + ")이 연결되어 있지 않습니다.\n포트 설정에서 먼저 연결하세요.",
                "WiFi 설정", MessageBoxButtons.OK, MessageBoxIcon.Warning);
            return false;
        }

        private void ApplyConfigToUi(NetConfig cfg)
        {
            _ssidBox.Text = cfg.Ssid;
            _changePasswordCheck.Checked = false;
            _passwordBox.Text = string.Empty;
            _serverIpBox.Text = cfg.ServerIp;
            _serverPortBox.Value = Math.Max(_serverPortBox.Minimum, Math.Min(_serverPortBox.Maximum, (decimal)cfg.ServerPort));
            _dhcpCheck.Checked = cfg.DhcpEnabled;
            _staticIpBox.Text = cfg.StaticIp;
            _gatewayBox.Text = cfg.Gateway;
            _maskBox.Text = cfg.Netmask;
        }

        private NetConfig ReadConfigFromUi()
        {
            return new NetConfig
            {
                Ssid = _ssidBox.Text.Trim(),
                Password = _changePasswordCheck.Checked ? _passwordBox.Text : string.Empty,
                ServerIp = _serverIpBox.Text.Trim(),
                ServerPort = (int)_serverPortBox.Value,
                DhcpEnabled = _dhcpCheck.Checked,
                StaticIp = _staticIpBox.Text.Trim(),
                Gateway = _gatewayBox.Text.Trim(),
                Netmask = _maskBox.Text.Trim()
            };
        }

        private void ChannelUsb_CheckedChanged(object sender, EventArgs e)
        {
            if (_channelUsb.Checked && _settings != null)
            {
                _settings.WifiCommandChannel = "Usb";
            }
        }

        private void ChannelUart_CheckedChanged(object sender, EventArgs e)
        {
            if (_channelUart.Checked && _settings != null)
            {
                _settings.WifiCommandChannel = "Uart";
            }
        }

        private void ChangePasswordCheck_CheckedChanged(object sender, EventArgs e)
        {
            _passwordBox.Enabled = _changePasswordCheck.Checked;
        }

        private void DhcpCheck_CheckedChanged(object sender, EventArgs e)
        {
            bool staticEnabled = !_dhcpCheck.Checked;
            _staticIpBox.Enabled = staticEnabled;
            _gatewayBox.Enabled = staticEnabled;
            _maskBox.Enabled = staticEnabled;
        }

        private void CmdTimeoutBox_ValueChanged(object sender, EventArgs e)
        {
            if (_settings != null)
            {
                _settings.WifiCommandTimeoutMs = (int)_cmdTimeoutBox.Value;
            }
        }

        private async void ReadButton_Click(object sender, EventArgs e)
        {
            if (!EnsureConnected())
            {
                return;
            }
            try
            {
                Log("GET CONFIG 요청...");
                NetConfig cfg = await Stm32Commands.GetConfigAsync(SelectedLink, (int)_cmdTimeoutBox.Value);
                ApplyConfigToUi(cfg);
                Log("GET CONFIG 완료");
            }
            catch (Exception ex)
            {
                Log("GET CONFIG 실패: " + ex.Message);
                MessageBox.Show(this, ex.Message, "현재값 읽기 실패", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        private async void SaveButton_Click(object sender, EventArgs e)
        {
            if (!EnsureConnected())
            {
                return;
            }

            NetConfig cfg = ReadConfigFromUi();
            if (string.IsNullOrEmpty(cfg.Ssid))
            {
                MessageBox.Show(this, "SSID를 입력하세요.", "WiFi 설정", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return;
            }
            if (!cfg.DhcpEnabled && (string.IsNullOrEmpty(cfg.StaticIp) || string.IsNullOrEmpty(cfg.Gateway) || string.IsNullOrEmpty(cfg.Netmask)))
            {
                MessageBox.Show(this, "DHCP를 사용하지 않을 경우 정적 IP/Gateway/Netmask를 모두 입력하세요.", "WiFi 설정", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return;
            }

            try
            {
                Log("설정 저장 시작...");
                await Stm32Commands.SetConfigAsync(SelectedLink, cfg, (int)_cmdTimeoutBox.Value, Log);
                Log("설정 저장 완료 (MCU가 자동으로 WiFi 재접속을 시도합니다)");
                MessageBox.Show(this, "저장되었습니다.", "WiFi 설정", MessageBoxButtons.OK, MessageBoxIcon.Information);
            }
            catch (Exception ex)
            {
                Log("설정 저장 실패: " + ex.Message);
                MessageBox.Show(this, ex.Message, "저장 실패", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        private async void StatusButton_Click(object sender, EventArgs e)
        {
            if (!EnsureConnected())
            {
                return;
            }
            try
            {
                int statusNumber = await Stm32Commands.GetStatusAsync(SelectedLink, (int)_cmdTimeoutBox.Value);
                string text = "STATUS " + statusNumber + " (" + Stm32Protocol.DescribeStatus(statusNumber) + ")";
                _statusValueLabel.Text = text;
                Log(text);
            }
            catch (Exception ex)
            {
                Log("STATUS 조회 실패: " + ex.Message);
                MessageBox.Show(this, ex.Message, "상태 조회 실패", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }
    }
}
