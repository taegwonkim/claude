using System;
using System.IO.Ports;
using System.Windows.Forms;
using Stm32WifiConfigTool.Services;

namespace Stm32WifiConfigTool.Forms
{
    /// <summary>
    /// COM 포트 선택, Baud Rate, 읽기/쓰기 통신 타임아웃 설정 창.
    /// USB(CDC)와 UART(USART3, 보통 USB-시리얼 변환기 경유)를 각각 독립적으로 연결/해제한다.
    /// </summary>
    public class PortSettingsForm : Form
    {
        private readonly ConnectionManager _conn;
        private readonly ChannelPanel _usbPanel;
        private readonly ChannelPanel _uartPanel;

        public PortSettingsForm(ConnectionManager conn)
        {
            _conn = conn;

            Text = "포트 설정";
            Width = 460;
            Height = 380;
            FormBorderStyle = FormBorderStyle.FixedDialog;
            MaximizeBox = false;
            MinimizeBox = false;
            StartPosition = FormStartPosition.CenterParent;

            var layout = new TableLayoutPanel
            {
                Dock = DockStyle.Fill,
                ColumnCount = 1,
                RowCount = 2,
                Padding = new Padding(10)
            };
            layout.RowStyles.Add(new RowStyle(SizeType.Percent, 50));
            layout.RowStyles.Add(new RowStyle(SizeType.Percent, 50));

            _usbPanel = new ChannelPanel("USB (CDC)", _conn.Usb, 115200);
            _uartPanel = new ChannelPanel("UART (USART3)", _conn.Uart, 115200);

            layout.Controls.Add(_usbPanel, 0, 0);
            layout.Controls.Add(_uartPanel, 0, 1);

            Controls.Add(layout);

            FormClosed += (s, e) =>
            {
                _usbPanel.Dispose();
                _uartPanel.Dispose();
            };
        }

        /// <summary>채널 하나(USB 또는 UART)의 포트/보레이트/타임아웃/연결 UI.</summary>
        private sealed class ChannelPanel : GroupBox
        {
            private readonly SerialLinkService _link;
            private readonly ComboBox _portCombo;
            private readonly ComboBox _baudCombo;
            private readonly NumericUpDown _readTimeout;
            private readonly NumericUpDown _writeTimeout;
            private readonly Button _connectButton;
            private readonly Label _statusLabel;

            public ChannelPanel(string title, SerialLinkService link, int defaultBaud)
            {
                _link = link;
                Text = title;
                Dock = DockStyle.Fill;
                Padding = new Padding(8);

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
                _baudCombo.Items.AddRange(new object[] { 9600, 19200, 38400, 57600, 115200, 230400 });
                _baudCombo.SelectedItem = defaultBaud;

                _readTimeout = new NumericUpDown { Dock = DockStyle.Fill, Minimum = 100, Maximum = 60000, Increment = 100, Value = 3000 };
                _writeTimeout = new NumericUpDown { Dock = DockStyle.Fill, Minimum = 100, Maximum = 60000, Increment = 100, Value = 2000 };

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
                string current = _portCombo.SelectedItem as string;
                _portCombo.Items.Clear();
                _portCombo.Items.AddRange(SerialPort.GetPortNames());
                if (current != null && _portCombo.Items.Contains(current))
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
