using System;
using System.Drawing;
using System.Windows.Forms;
using Stm32WifiConfigTool.Forms;
using Stm32WifiConfigTool.Services;

namespace Stm32WifiConfigTool
{
    /// <summary>
    /// STM32L562C WiFi 계측 브릿지 PC 도구의 진입 창. USB/UART 연결을 소유(ConnectionManager)하고,
    /// 포트 설정/WiFi 설정/측정값 보기 3개 창을 연다(창마다 1개 인스턴스만 유지, 이미 열려있으면 활성화).
    /// </summary>
    public class MainForm : Form
    {
        private readonly ConnectionManager _conn = new ConnectionManager();

        private PortSettingsForm _portSettingsForm;
        private WifiConfigForm _wifiConfigForm;
        private MeasurementForm _measurementForm;

        private readonly Label _usbStatusLabel;
        private readonly Label _uartStatusLabel;

        public MainForm()
        {
            Text = "STM32L562C WiFi 계측 브릿지 도구";
            Width = 380;
            Height = 300;
            FormBorderStyle = FormBorderStyle.FixedSingle;
            MaximizeBox = false;
            StartPosition = FormStartPosition.CenterScreen;

            var layout = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 1, Padding = new Padding(16) };

            var title = new Label
            {
                Text = "STM32L562C WiFi 계측 브릿지",
                Dock = DockStyle.Top,
                Font = new Font(Font.FontFamily, 11f, FontStyle.Bold),
                Height = 30
            };
            layout.Controls.Add(title);

            var openPortButton = new Button { Text = "① 포트 설정...", Dock = DockStyle.Top, Height = 36, Margin = new Padding(0, 8, 0, 4) };
            openPortButton.Click += (s, e) => ShowSingleton(ref _portSettingsForm, () => new PortSettingsForm(_conn));
            layout.Controls.Add(openPortButton);

            var openWifiButton = new Button { Text = "② WiFi 설정...", Dock = DockStyle.Top, Height = 36, Margin = new Padding(0, 4, 0, 4) };
            openWifiButton.Click += (s, e) => ShowSingleton(ref _wifiConfigForm, () => new WifiConfigForm(_conn));
            layout.Controls.Add(openWifiButton);

            var openMeasurementButton = new Button { Text = "③ 측정값 보기...", Dock = DockStyle.Top, Height = 36, Margin = new Padding(0, 4, 0, 4) };
            openMeasurementButton.Click += (s, e) => ShowSingleton(ref _measurementForm, () => new MeasurementForm(_conn));
            layout.Controls.Add(openMeasurementButton);

            var statusGroup = new GroupBox { Text = "연결 상태", Dock = DockStyle.Top, Height = 80, Margin = new Padding(0, 12, 0, 0) };
            _usbStatusLabel = new Label { Text = "USB: 연결 안됨", Left = 12, Top = 22, AutoSize = true, ForeColor = Color.Firebrick };
            _uartStatusLabel = new Label { Text = "UART: 연결 안됨", Left = 12, Top = 46, AutoSize = true, ForeColor = Color.Firebrick };
            statusGroup.Controls.Add(_usbStatusLabel);
            statusGroup.Controls.Add(_uartStatusLabel);
            layout.Controls.Add(statusGroup);

            Controls.Add(layout);

            _conn.Usb.ConnectionChanged += OnLinkConnectionChanged;
            _conn.Uart.ConnectionChanged += OnLinkConnectionChanged;

            FormClosed += (s, e) => _conn.Dispose();
        }

        private void OnLinkConnectionChanged(LinkChannel channel, bool connected)
        {
            if (IsDisposed || !IsHandleCreated)
            {
                return;
            }
            BeginInvoke(new Action(() =>
            {
                Label label = channel == LinkChannel.Usb ? _usbStatusLabel : _uartStatusLabel;
                string name = channel == LinkChannel.Usb ? "USB" : "UART";
                label.Text = name + ": " + (connected ? "연결됨" : "연결 안됨");
                label.ForeColor = connected ? Color.SeaGreen : Color.Firebrick;
            }));
        }

        /// <summary>지정한 폼을 1개만 유지: 이미 열려있으면 활성화, 아니면 새로 생성해 표시한다.</summary>
        private void ShowSingleton<T>(ref T formField, Func<T> factory) where T : Form
        {
            if (formField == null || formField.IsDisposed)
            {
                formField = factory();
                formField.Owner = this;
                formField.Show();
            }
            else
            {
                formField.Activate();
                if (formField.WindowState == FormWindowState.Minimized)
                {
                    formField.WindowState = FormWindowState.Normal;
                }
            }
        }
    }
}
