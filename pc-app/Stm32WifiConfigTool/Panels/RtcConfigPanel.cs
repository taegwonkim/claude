using System;
using System.Windows.Forms;
using Stm32WifiConfigTool.Models;
using Stm32WifiConfigTool.Services;

namespace Stm32WifiConfigTool.Panels
{
    /// <summary>
    /// RTC 관련 설정 패널. 두 개의 독립된 그룹이 있다:
    /// (1) "RTC 리셋 설정" - "Read"로 MCU에 RESET_R_ALL을 보내 현재 설정된 리셋 주기(초)를 화면에
    /// 채우고, "Write"로 입력값을 RESET_W_ALL 한 프레임에 담아 MCU에 전달한다(docs/프로토콜_명세.md
    /// §6, firmware/firmware-no-rtos 양쪽 이미 구현됨).
    /// (2) "시/분/초 개별 설정" - RTC_R_H/RTC_R_M/RTC_R_S를 순서대로 보내 시/분/초를 각각 조회하고,
    /// RTC_W_H/RTC_W_M/RTC_W_S로 각각 전달한다. (1)과는 완전히 별도의 값이며 자체 Read/Write
    /// 버튼(<c>_unitReadButton</c>/<c>_unitWriteButton</c>)을 따로 쓴다 - 채널 선택/커맨드
    /// 타임아웃/로그는 두 그룹이 함께 쓴다.
    /// "Read" 성공 시 값을 <see cref="AppSettings"/>에 캐시해두고, 다음 실행 시
    /// <see cref="Initialize"/>가 이를 화면에 미리 채운다(MCU 재조회 전 참고용).
    /// UI 레이아웃은 <c>RtcConfigPanel.Designer.cs</c>에 있으며 Visual Studio 디자이너로 편집 가능하다.
    /// 매개변수 없는 생성자는 디자이너 전용이며, 실제 사용 시에는 생성 직후 <see cref="Initialize"/>를
    /// 호출해 런타임 의존성(ConnectionManager, AppSettings)을 연결해야 한다.
    /// </summary>
    public partial class RtcConfigPanel : UserControl
    {
        private ConnectionManager _conn;
        private AppSettings _settings;

        /// <summary>"분"/"초" 콤보박스에 채워 넣는 값의 최댓값(0부터 이 값까지).</summary>
        private const int MinuteSecondComboMax = 59;

        /// <summary>"시간" 콤보박스에 채워 넣는 값의 최댓값(0부터 이 값까지).</summary>
        private const int HourComboMax = 99;

        public RtcConfigPanel()
        {
            InitializeComponent();
            PopulateUnitCombos();
        }

        /// <summary>_secBox/_minBox/_hourBox에 0부터 각 최댓값까지의 정수를 항목으로 채워 넣고
        /// 0을 기본 선택한다 - 콤보박스 항목 인덱스가 곧 그 값이므로(Items[i] == i), 값을 읽고
        /// 쓸 때 단순히 SelectedIndex를 쓰면 된다.</summary>
        private void PopulateUnitCombos()
        {
            for (int i = 0; i <= HourComboMax; i++)
            {
                _hourBox.Items.Add(i);
            }
            for (int i = 0; i <= MinuteSecondComboMax; i++)
            {
                _minBox.Items.Add(i);
                _secBox.Items.Add(i);
            }
            _hourBox.SelectedIndex = 0;
            _minBox.SelectedIndex = 0;
            _secBox.SelectedIndex = 0;
        }

        /// <summary>value를 [0, max] 범위로 자르고, 콤보박스 SelectedIndex로 바로 쓸 수 있는
        /// 인덱스를 반환한다(항목 인덱스가 곧 값이므로 그대로 반환).</summary>
        private static int ClampIndex(int value, int max)
        {
            if (value < 0) return 0;
            if (value > max) return max;
            return value;
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

            /* 마지막으로 "Read"에 성공했던 값을 화면에 미리 채운다 - MCU를 다시 조회하기 전까지
             * 참고용이며, 실제 값의 원본은 항상 MCU다. */
            _periodBox.Value = ClampDecimal(settings.RtcPeriodSecCache, _periodBox.Minimum, _periodBox.Maximum);
            _hourBox.SelectedIndex = ClampIndex(settings.RtcHourCache, HourComboMax);
            _minBox.SelectedIndex = ClampIndex(settings.RtcMinuteCache, MinuteSecondComboMax);
            _secBox.SelectedIndex = ClampIndex(settings.RtcSecondCache, MinuteSecondComboMax);
        }

        private static decimal ClampDecimal(int value, decimal min, decimal max)
        {
            if (value < min) return min;
            if (value > max) return max;
            return value;
        }

        private SerialLinkService SelectedLink => _channelUsb.Checked ? _conn.Usb : _conn.Uart;

        /// <summary>"Read"(리셋 주기)로 받은 값을 로컬 캐시에 저장하고 즉시 파일에 반영한다
        /// (다음 실행 시 <see cref="Initialize"/>가 이 값을 화면에 미리 채운다).</summary>
        private void SavePeriodCache(RtcConfig cfg)
        {
            _settings.RtcPeriodSecCache = cfg.PeriodSec;
            try
            {
                AppSettingsStore.Save(_settings);
            }
            catch (Exception)
            {
                /* 설정 저장 실패(권한/디스크 문제 등)로 UI 동작 자체가 막히면 안 되므로 무시 */
            }
        }

        /// <summary>"Read"(시/분/초)로 받은 값을 로컬 캐시에 저장하고 즉시 파일에 반영한다
        /// (다음 실행 시 <see cref="Initialize"/>가 이 값을 화면에 미리 채운다).</summary>
        private void SaveUnitCache(RtcConfig cfg)
        {
            _settings.RtcHourCache = cfg.Hour;
            _settings.RtcMinuteCache = cfg.Minute;
            _settings.RtcSecondCache = cfg.Second;
            try
            {
                AppSettingsStore.Save(_settings);
            }
            catch (Exception)
            {
                /* 설정 저장 실패(권한/디스크 문제 등)로 UI 동작 자체가 막히면 안 되므로 무시 */
            }
        }

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
                SavePeriodCache(cfg);
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

        /// <summary>RTC_R_H/RTC_R_M/RTC_R_S를 순서대로 보내 시/분/초를 조회해 화면에 채운다
        /// (위 "리셋 주기(초)"와는 완전히 별도의 값).</summary>
        private async void UnitReadButton_Click(object sender, EventArgs e)
        {
            if (!EnsureConnected())
            {
                return;
            }
            try
            {
                Log("RTC_R_H/RTC_R_M/RTC_R_S 요청...");
                RtcConfig cfg = await Stm32Commands.GetRtcUnitsAsync(SelectedLink, (int)_cmdTimeoutBox.Value);
                _hourBox.SelectedIndex = ClampIndex(cfg.Hour, HourComboMax);
                _minBox.SelectedIndex = ClampIndex(cfg.Minute, MinuteSecondComboMax);
                _secBox.SelectedIndex = ClampIndex(cfg.Second, MinuteSecondComboMax);
                SaveUnitCache(cfg);
                Log("RTC_R_H/RTC_R_M/RTC_R_S 완료 (" + cfg.Hour + "시 " + cfg.Minute + "분 " + cfg.Second + "초)");
            }
            catch (Exception ex)
            {
                Log("RTC_R_H/RTC_R_M/RTC_R_S 실패: " + ex.Message);
                MessageBox.Show(this, ex.Message, "읽기 실패", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        /// <summary>화면의 시/분/초 입력값을 RTC_W_H/RTC_W_M/RTC_W_S 순서로 MCU에 전달한다.</summary>
        private async void UnitWriteButton_Click(object sender, EventArgs e)
        {
            if (!EnsureConnected())
            {
                return;
            }

            var cfg = new RtcConfig
            {
                Hour = _hourBox.SelectedIndex,
                Minute = _minBox.SelectedIndex,
                Second = _secBox.SelectedIndex
            };
            try
            {
                Log("RTC_W_H/RTC_W_M/RTC_W_S 전송... (" + cfg.Hour + "시 " + cfg.Minute + "분 " + cfg.Second + "초)");
                await Stm32Commands.SetRtcUnitsAsync(SelectedLink, cfg, (int)_cmdTimeoutBox.Value);
                Log("RTC_W_H/RTC_W_M/RTC_W_S 완료");
                MessageBox.Show(this, "전달되었습니다.", "RTC 설정", MessageBoxButtons.OK, MessageBoxIcon.Information);
            }
            catch (Exception ex)
            {
                Log("RTC_W_H/RTC_W_M/RTC_W_S 실패: " + ex.Message);
                MessageBox.Show(this, ex.Message, "쓰기 실패", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }
    }
}
