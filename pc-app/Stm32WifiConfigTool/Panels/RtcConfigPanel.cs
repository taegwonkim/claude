using System;
using System.Windows.Forms;
using Stm32WifiConfigTool.Models;
using Stm32WifiConfigTool.Services;

namespace Stm32WifiConfigTool.Panels
{
    /// <summary>
    /// RTC Wakeup Timer 기반 주기적 리셋 설정 패널. "Read"로 MCU에 RESET_R_ALL을 보내 현재
    /// 설정된 리셋 주기(초)를 화면에 채우고, "Write"로 입력값을 RESET_W_ALL 한 프레임에 담아
    /// MCU에 전달한다(docs/프로토콜_명세.md §6, firmware/firmware-no-rtos 양쪽 이미 구현됨).
    /// UI 레이아웃은 <c>RtcConfigPanel.Designer.cs</c>에 있으며 Visual Studio 디자이너로 편집 가능하다.
    /// 매개변수 없는 생성자는 디자이너 전용이며, 실제 사용 시에는 생성 직후 <see cref="Initialize"/>를
    /// 호출해 런타임 의존성(ConnectionManager, AppSettings)을 연결해야 한다.
    /// </summary>
    public partial class RtcConfigPanel : UserControl
    {
        private ConnectionManager _conn;
        private AppSettings _settings;

        public RtcConfigPanel()
        {
            InitializeComponent();
        }

        /// <summary>디자이너가 만든 컨트롤에 실제 동작을 연결한다. MainForm이 생성 직후 1회 호출.</summary>
        public void Initialize(ConnectionManager conn, AppSettings settings)
        {
            _conn = conn;
            _settings = settings;

            bool useUart = settings.RtcConfigCommandChannel == "Uart";
            _channelUsb.Checked = !useUart;
            _channelUart.Checked = useUart;

            _cmdTimeoutBox.Value = ClampDecimal(settings.RtcConfigCommandTimeoutMs, _cmdTimeoutBox.Minimum, _cmdTimeoutBox.Maximum);
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
                "RTC 설정", MessageBoxButtons.OK, MessageBoxIcon.Warning);
            return false;
        }

        private void ChannelUsb_CheckedChanged(object sender, EventArgs e)
        {
            if (_channelUsb.Checked && _settings != null)
            {
                _settings.RtcConfigCommandChannel = "Usb";
            }
        }

        private void ChannelUart_CheckedChanged(object sender, EventArgs e)
        {
            if (_channelUart.Checked && _settings != null)
            {
                _settings.RtcConfigCommandChannel = "Uart";
            }
        }

        private void CmdTimeoutBox_ValueChanged(object sender, EventArgs e)
        {
            if (_settings != null)
            {
                _settings.RtcConfigCommandTimeoutMs = (int)_cmdTimeoutBox.Value;
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
                Log("RESET_R_ALL 요청...");
                RtcConfig cfg = await Stm32Commands.GetResetAllAsync(SelectedLink, (int)_cmdTimeoutBox.Value);
                _periodBox.Value = ClampDecimal(cfg.PeriodSec, _periodBox.Minimum, _periodBox.Maximum);
                Log("RESET_R_ALL 완료 (" + cfg.PeriodSec + "초)");
            }
            catch (Exception ex)
            {
                Log("RESET_R_ALL 실패: " + ex.Message);
                MessageBox.Show(this, ex.Message, "읽기 실패", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        private async void WriteButton_Click(object sender, EventArgs e)
        {
            if (!EnsureConnected())
            {
                return;
            }

            var cfg = new RtcConfig { PeriodSec = (int)_periodBox.Value };
            try
            {
                Log("RESET_W_ALL 전송... (" + cfg.PeriodSec + "초)");
                await Stm32Commands.SetResetAllAsync(SelectedLink, cfg, (int)_cmdTimeoutBox.Value);
                Log("RESET_W_ALL 완료");
                MessageBox.Show(this, "전달되었습니다.", "RTC 설정", MessageBoxButtons.OK, MessageBoxIcon.Information);
            }
            catch (Exception ex)
            {
                Log("RESET_W_ALL 실패: " + ex.Message);
                MessageBox.Show(this, ex.Message, "쓰기 실패", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }
    }
}
