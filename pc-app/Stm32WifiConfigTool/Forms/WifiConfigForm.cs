using System;
using System.Drawing;
using System.Windows.Forms;
using Stm32WifiConfigTool.Models;
using Stm32WifiConfigTool.Services;

namespace Stm32WifiConfigTool.Forms
{
    /// <summary>
    /// WiFi(AP SSID/Password), 서버 IP/Port, DHCP/정적 IP 설정 창.
    /// "현재값 읽기"로 MCU의 GET CONFIG 응답을 화면에 채우고, "저장"으로 SET+SAVE를 순차 전송한다.
    /// </summary>
    public class WifiConfigForm : Form
    {
        private readonly ConnectionManager _conn;

        private readonly RadioButton _channelUsb;
        private readonly RadioButton _channelUart;

        private readonly TextBox _ssidBox;
        private readonly CheckBox _changePasswordCheck;
        private readonly TextBox _passwordBox;
        private readonly TextBox _serverIpBox;
        private readonly NumericUpDown _serverPortBox;
        private readonly CheckBox _dhcpCheck;
        private readonly TextBox _staticIpBox;
        private readonly TextBox _gatewayBox;
        private readonly TextBox _maskBox;
        private readonly Label _statusValueLabel;
        private readonly NumericUpDown _cmdTimeoutBox;
        private readonly TextBox _logBox;

        public WifiConfigForm(ConnectionManager conn)
        {
            _conn = conn;

            Text = "WiFi 설정";
            Width = 520;
            Height = 620;
            FormBorderStyle = FormBorderStyle.FixedDialog;
            MaximizeBox = false;
            MinimizeBox = false;
            StartPosition = FormStartPosition.CenterParent;

            var root = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 1, Padding = new Padding(10) };
            root.RowStyles.Add(new RowStyle(SizeType.AutoSize));
            root.RowStyles.Add(new RowStyle(SizeType.AutoSize));
            root.RowStyles.Add(new RowStyle(SizeType.Percent, 100));

            // 채널 선택
            var channelGroup = new GroupBox { Text = "명령 전송 채널", Dock = DockStyle.Top, Height = 55 };
            _channelUsb = new RadioButton { Text = "USB", Left = 15, Top = 22, Checked = true, AutoSize = true };
            _channelUart = new RadioButton { Text = "UART", Left = 100, Top = 22, AutoSize = true };
            channelGroup.Controls.Add(_channelUsb);
            channelGroup.Controls.Add(_channelUart);
            root.Controls.Add(channelGroup, 0, 0);

            // 설정 필드
            var fieldsGroup = new GroupBox { Text = "설정값", Dock = DockStyle.Top, Height = 330 };
            var fields = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 2, Padding = new Padding(8) };
            fields.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 130));
            fields.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));

            _ssidBox = new TextBox { Dock = DockStyle.Fill, MaxLength = 31 };
            _changePasswordCheck = new CheckBox { Text = "비밀번호 변경", AutoSize = true };
            _passwordBox = new TextBox { Dock = DockStyle.Fill, MaxLength = 63, UseSystemPasswordChar = true, Enabled = false };
            _changePasswordCheck.CheckedChanged += (s, e) => _passwordBox.Enabled = _changePasswordCheck.Checked;

            var passwordRow = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 2 };
            passwordRow.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
            passwordRow.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
            passwordRow.Controls.Add(_passwordBox, 0, 0);
            passwordRow.Controls.Add(_changePasswordCheck, 1, 0);

            _serverIpBox = new TextBox { Dock = DockStyle.Fill };
            _serverPortBox = new NumericUpDown { Dock = DockStyle.Fill, Minimum = 0, Maximum = 65535, Value = 50001 };

            _dhcpCheck = new CheckBox { Text = "DHCP 사용", AutoSize = true, Checked = true };
            _staticIpBox = new TextBox { Dock = DockStyle.Fill, Enabled = false };
            _gatewayBox = new TextBox { Dock = DockStyle.Fill, Enabled = false };
            _maskBox = new TextBox { Dock = DockStyle.Fill, Enabled = false };
            _dhcpCheck.CheckedChanged += (s, e) =>
            {
                bool staticEnabled = !_dhcpCheck.Checked;
                _staticIpBox.Enabled = staticEnabled;
                _gatewayBox.Enabled = staticEnabled;
                _maskBox.Enabled = staticEnabled;
            };

            int row = 0;
            AddRow(fields, ref row, "SSID", _ssidBox);
            AddRow(fields, ref row, "비밀번호", passwordRow);
            AddRow(fields, ref row, "서버 IP", _serverIpBox);
            AddRow(fields, ref row, "서버 Port", _serverPortBox);
            AddRow(fields, ref row, string.Empty, _dhcpCheck);
            AddRow(fields, ref row, "정적 IP", _staticIpBox);
            AddRow(fields, ref row, "Gateway", _gatewayBox);
            AddRow(fields, ref row, "Netmask", _maskBox);

            fieldsGroup.Controls.Add(fields);
            root.Controls.Add(fieldsGroup, 0, 1);

            // 하단: 버튼/상태/로그
            var bottom = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 1 };
            bottom.RowStyles.Add(new RowStyle(SizeType.AutoSize));
            bottom.RowStyles.Add(new RowStyle(SizeType.AutoSize));
            bottom.RowStyles.Add(new RowStyle(SizeType.Percent, 100));

            var buttonRow = new FlowLayoutPanel { Dock = DockStyle.Top, FlowDirection = FlowDirection.LeftToRight, AutoSize = true, WrapContents = false };
            var readButton = new Button { Text = "현재값 읽기", AutoSize = true };
            var saveButton = new Button { Text = "저장 (SET+SAVE)", AutoSize = true };
            var statusButton = new Button { Text = "상태 조회", AutoSize = true };
            readButton.Click += ReadButton_Click;
            saveButton.Click += SaveButton_Click;
            statusButton.Click += StatusButton_Click;
            buttonRow.Controls.Add(readButton);
            buttonRow.Controls.Add(saveButton);
            buttonRow.Controls.Add(statusButton);
            buttonRow.Controls.Add(new Label { Text = "  커맨드 타임아웃(ms)", AutoSize = true, TextAlign = ContentAlignment.MiddleLeft, Padding = new Padding(0, 8, 0, 0) });
            _cmdTimeoutBox = new NumericUpDown { Minimum = 200, Maximum = 30000, Increment = 100, Value = 3000, Width = 80 };
            buttonRow.Controls.Add(_cmdTimeoutBox);
            bottom.Controls.Add(buttonRow, 0, 0);

            _statusValueLabel = new Label { Text = "STATUS: -", AutoSize = true, Dock = DockStyle.Top, Padding = new Padding(0, 6, 0, 6) };
            bottom.Controls.Add(_statusValueLabel, 0, 1);

            _logBox = new TextBox
            {
                Dock = DockStyle.Fill,
                Multiline = true,
                ReadOnly = true,
                ScrollBars = ScrollBars.Vertical,
                Font = new Font(FontFamily.GenericMonospace, 8.5f)
            };
            bottom.Controls.Add(_logBox, 0, 2);

            root.Controls.Add(bottom, 0, 2);

            Controls.Add(root);
        }

        private static void AddRow(TableLayoutPanel layout, ref int row, string labelText, Control control)
        {
            layout.RowCount = row + 1;
            layout.RowStyles.Add(new RowStyle(SizeType.Absolute, 32));
            if (!string.IsNullOrEmpty(labelText))
            {
                layout.Controls.Add(new Label { Text = labelText, Dock = DockStyle.Fill, TextAlign = ContentAlignment.MiddleLeft }, 0, row);
            }
            layout.Controls.Add(control, 1, row);
            row++;
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
            MessageBox.Show(this, "선택한 채널(" + (_channelUsb.Checked ? "USB" : "UART") + ")이 연결되어 있지 않습니다.\n포트 설정 창에서 먼저 연결하세요.",
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
                string reply = await Stm32Commands.GetStatusAsync(SelectedLink, (int)_cmdTimeoutBox.Value);
                _statusValueLabel.Text = reply;
                Log(reply);
            }
            catch (Exception ex)
            {
                Log("STATUS 조회 실패: " + ex.Message);
                MessageBox.Show(this, ex.Message, "상태 조회 실패", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }
    }
}
