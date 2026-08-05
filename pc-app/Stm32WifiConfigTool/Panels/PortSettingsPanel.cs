using System;
using System.IO.Ports;
using System.Windows.Forms;
using Stm32WifiConfigTool.Models;
using Stm32WifiConfigTool.Services;

namespace Stm32WifiConfigTool.Panels
{
    /// <summary>
    /// COM 포트 선택, Baud Rate, 읽기/쓰기 통신 타임아웃 설정 패널.
    /// USB(CDC)와 UART(USART3, 보통 USB-시리얼 변환기 경유)를 각각 독립적으로 연결/해제한다.
    /// 값이 바뀔 때마다 전달받은 <see cref="ChannelSettings"/>에 실시간 반영되며, 앱 종료 시
    /// MainForm이 AppSettingsStore.Save()로 파일에 저장해 다음 실행에서 그대로 복원된다.
    /// MainForm에 다른 패널들과 함께 한 창에 도킹되어 표시된다.
    /// </summary>
    public class PortSettingsPanel : UserControl
    {
        public PortSettingsPanel(ConnectionManager conn, AppSettings settings)
        {
            var layout = new TableLayoutPanel
            {
                Dock = DockStyle.Fill,
                ColumnCount = 1,
                RowCount = 2,
                Padding = new System.Windows.Forms.Padding(6)
            };
            layout.RowStyles.Add(new RowStyle(SizeType.Percent, 50));
            layout.RowStyles.Add(new RowStyle(SizeType.Percent, 50));

            var usbPanel = new ChannelPanel("USB (CDC)", conn.Usb, settings.Usb) { Dock = DockStyle.Fill };
            var uartPanel = new ChannelPanel("UART (USART3)", conn.Uart, settings.Uart) { Dock = DockStyle.Fill };

            layout.Controls.Add(usbPanel, 0, 0);
            layout.Controls.Add(uartPanel, 0, 1);

            Controls.Add(layout);
        }

        /// <summary>채널 하나(USB 또는 UART)의 포트/보레이트/타임아웃/연결 UI.</summary>
        private sealed class ChannelPanel : GroupBox
        {
            private static readonly int[] BaudRates = { 9600, 19200, 38400, 57600, 115200, 230400 };

            private readonly SerialLinkService _link;
            private readonly ChannelSettings _settings;
            private readonly ComboBox _portCombo;
            private readonly ComboBox _baudCombo;
            private readonly NumericUpDown _readTimeout;
            private readonly NumericUpDown _writeTimeout;
            private readonly Button _connectButton;
            private readonly Label _statusLabel;

            public ChannelPanel(string title, SerialLinkService link, ChannelSettings settings)
            {
                _link = link;
                _settings = settings;
                Text = title;
                Dock = DockStyle.Fill;
                Padding = new System.Windows.Forms.Padding(8);

                var layout = new TableLayoutPanel
                {
                    Dock = DockStyle.Fill,
                    ColumnCount = 2,
                    RowCount = 6
                };
                layout.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 120));
                layout.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));

                _portCombo = new ComboBox { Dock = DockStyle.Fill, DropDownStyle = ComboBoxStyle.DropDownList };
                var refreshButton = new Button { Text = "새로고침", AutoSize = true };
                refreshButton.Click += (s, e) => RefreshPorts();

                var portRow = new FlowLayoutPanel { Dock = DockStyle.Fill, FlowDirection = FlowDirection.LeftToRight, WrapContents = false };
                portRow.Controls.Add(_portCombo);
                portRow.Controls.Add(refreshButton);
                _portCombo.Width = 150;

                _baudCombo = new ComboBox { Dock = DockStyle.Fill, DropDownStyle = ComboBoxStyle.DropDownList };
                _baudCombo.Items.AddRange(Array.ConvertAll(BaudRates, b => (object)b));
                _baudCombo.SelectedItem = Array.IndexOf(BaudRates, settings.BaudRate) >= 0 ? settings.BaudRate : 115200;

                _readTimeout = new NumericUpDown { Dock = DockStyle.Fill, Minimum = 100, Maximum = 60000, Increment = 100 };
                _readTimeout.Value = Clamp(settings.ReadTimeoutMs, _readTimeout.Minimum, _readTimeout.Maximum);
                _writeTimeout = new NumericUpDown { Dock = DockStyle.Fill, Minimum = 100, Maximum = 60000, Increment = 100 };
                _writeTimeout.Value = Clamp(settings.WriteTimeoutMs, _writeTimeout.Minimum, _writeTimeout.Maximum);

                _connectButton = new Button { Text = "연결", Dock = DockStyle.Fill };
                _connectButton.Click += ConnectButton_Click;

                _statusLabel = new Label { Text = "연결 안됨", Dock = DockStyle.Fill, ForeColor = System.Drawing.Color.Firebrick, TextAlign = System.Drawing.ContentAlignment.MiddleLeft };

                int row = 0;
                AddRow(layout, row++, "포트", portRow);
                AddRow(layout, row++, "Baud Rate", _baudCombo);
                AddRow(layout, row++, "읽기 타임아웃(ms)", _readTimeout);
                AddRow(layout, row++, "쓰기 타임아웃(ms)", _writeTimeout);
                AddRow(layout, row++, string.Empty, _connectButton);
                AddRow(layout, row++, "상태", _statusLabel);

                Controls.Add(layout);

                _link.ConnectionChanged += Link_ConnectionChanged;

                RefreshPorts();

                // 값이 바뀔 때마다 전달받은 ChannelSettings에 실시간 반영 (저장은 앱 종료 시 MainForm이 일괄 수행)
                _portCombo.SelectedIndexChanged += (s, e) =>
                {
                    if (_portCombo.SelectedItem is string port)
                    {
                        _settings.PortName = port;
                    }
                };
                _baudCombo.SelectedIndexChanged += (s, e) =>
                {
                    if (_baudCombo.SelectedItem is int baud)
                    {
                        _settings.BaudRate = baud;
                    }
                };
                _readTimeout.ValueChanged += (s, e) => _settings.ReadTimeoutMs = (int)_readTimeout.Value;
                _writeTimeout.ValueChanged += (s, e) => _settings.WriteTimeoutMs = (int)_writeTimeout.Value;
            }

            private static decimal Clamp(int value, decimal min, decimal max)
            {
                if (value < min) return min;
                if (value > max) return max;
                return value;
            }

            private static void AddRow(TableLayoutPanel layout, int row, string labelText, Control control)
            {
                layout.RowStyles.Add(new RowStyle(SizeType.Absolute, 30));
                if (!string.IsNullOrEmpty(labelText))
                {
                    layout.Controls.Add(new Label { Text = labelText, Dock = DockStyle.Fill, TextAlign = System.Drawing.ContentAlignment.MiddleLeft }, 0, row);
                }
                layout.Controls.Add(control, 1, row);
            }

            private void RefreshPorts()
            {
                string current = _portCombo.SelectedItem as string ?? _settings.PortName;
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
                    _link.ConnectionChanged -= Link_ConnectionChanged;
                }
                base.Dispose(disposing);
            }
        }
    }
}
