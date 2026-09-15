using System;
using System.Windows.Forms;
using Stm32WifiConfigTool.Models;
using Stm32WifiConfigTool.Services;

namespace Stm32WifiConfigTool.Panels
{
    /// <summary>
    /// RTC 관련 설정 패널. "RTC 리셋 설정" 그룹 하나에 두 가지 값이 있고, 그 아래 Read/Write
    /// 버튼(<c>_readButton</c>/<c>_writeButton</c>) 하나로 둘 다 함께 처리한다(별도의 버튼이
    /// 더 있지 않다):
    /// (1) "리셋 주기" - RESET_R_ALL/RESET_W_ALL 프레임으로 MCU와 주고받는 리셋 주기(초)
    /// (docs/프로토콜_명세.md §6, firmware/firmware-no-rtos 양쪽 이미 구현됨).
    /// (2) "단위"(시/분/초) - RTC_R_H/RTC_R_M/RTC_R_S 또는 RTC_W_H/RTC_W_M/RTC_W_S 중 "단위"
    /// 콤보박스에서 고른 것 하나만 개별로 읽고 쓴다. 별도의 "값" 입력란은 없고, Write 시 위
    /// "리셋 주기"(초)를 시/분/초로 환산한 값 중 선택된 단위에 해당하는 값을 그대로 보낸다
    /// (<see cref="DecomposePeriod"/>).
    /// "Read"를 누르면 RESET_R_ALL로 리셋 주기를 먼저 읽어 화면에 채우고, 이어서 선택된 단위의
    /// RTC_R_x도 조회해 로그에 표시한다(별도 표시 입력란은 없다). "Write"를 누르면 화면의 리셋
    /// 주기를 RESET_W_ALL로 먼저 전달하고, 이어서 그 값을 시/분/초로 환산해 선택된 단위의
    /// RTC_W_x도 전송한다 - 즉 한 번의 클릭으로 (1)과 (2)가 항상 함께 처리된다
    /// (<see cref="UnitReadButton_Click"/>/<see cref="UnitWriteButton_Click"/>).
    /// "Read" 성공 시 리셋 주기 값을 <see cref="AppSettings"/>에 캐시해두고, 다음 실행 시
    /// <see cref="Initialize"/>가 이를 화면에 미리 채운다(MCU 재조회 전 참고용).
    /// UI 레이아웃은 <c>RtcConfigPanel.Designer.cs</c>에 있으며 Visual Studio 디자이너로 편집 가능하다.
    /// 매개변수 없는 생성자는 디자이너 전용이며, 실제 사용 시에는 생성 직후 <see cref="Initialize"/>를
    /// 호출해 런타임 의존성(ConnectionManager, AppSettings)을 연결해야 한다.
    /// </summary>
    public partial class RtcConfigPanel : UserControl
    {
        private ConnectionManager _conn;
        private AppSettings _settings;

        private const string UnitKindHour = "시";
        private const string UnitKindMinute = "분";
        private const string UnitKindSecond = "초";

        /// <summary>"단위" 콤보박스("_unitKindBox")의 항목 - 순서가 곧 표시 순서다.</summary>
        private static readonly string[] UnitKinds = { UnitKindHour, UnitKindMinute, UnitKindSecond };

        public RtcConfigPanel()
        {
            InitializeComponent();
            _unitKindBox.Items.AddRange(UnitKinds);
            _unitKindBox.SelectedIndex = 0;
        }

        /// <summary>_periodBox의 현재 값(초)을 시/분/초로 환산한다(예: 5000초 → 1시 23분 20초).</summary>
        private void DecomposePeriod(out int hour, out int minute, out int second)
        {
            int total = (int)_periodBox.Value;
            hour = total / 3600;
            int remainder = total % 3600;
            minute = remainder / 60;
            second = remainder % 60;
        }

        private void UnitKindBox_SelectedIndexChanged(object sender, EventArgs e)
        {
            if (_settings != null)
            {
                _settings.RtcUnitKindCache = (string)_unitKindBox.SelectedItem;
            }
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

            /* 마지막으로 선택했던 단위(시/분/초)를 미리 고른다 - 목록에 없는 값이 저장돼 있으면
             * (예: 설정 파일 손상) 첫 항목("시")으로 대체한다. */
            int kindIndex = Array.IndexOf(UnitKinds, settings.RtcUnitKindCache);
            _unitKindBox.SelectedIndex = kindIndex >= 0 ? kindIndex : 0;
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

        /// <summary>이 패널의 유일한 Read 버튼: RESET_R_ALL로 리셋 주기를 먼저 읽어 화면에
        /// 채우고 캐시한 뒤, 이어서 _unitKindBox에서 선택된 단위 하나(RTC_R_H/RTC_R_M/RTC_R_S 중
        /// 해당하는 것)도 조회해 로그에 표시한다(별도 표시 입력란은 없다). 리셋 주기 읽기가
        /// 실패하면 단위 읽기는 시도하지 않는다.</summary>
        private async void UnitReadButton_Click(object sender, EventArgs e)
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
                return;
            }

            string kind = (string)_unitKindBox.SelectedItem;
            try
            {
                int value;
                switch (kind)
                {
                    case UnitKindHour:
                        Log("RTC_R_H 요청...");
                        value = await Stm32Commands.GetRtcHourAsync(SelectedLink, (int)_cmdTimeoutBox.Value);
                        break;
                    case UnitKindMinute:
                        Log("RTC_R_M 요청...");
                        value = await Stm32Commands.GetRtcMinuteAsync(SelectedLink, (int)_cmdTimeoutBox.Value);
                        break;
                    default:
                        Log("RTC_R_S 요청...");
                        value = await Stm32Commands.GetRtcSecondAsync(SelectedLink, (int)_cmdTimeoutBox.Value);
                        break;
                }
                Log(kind + " 읽기 완료 (현재 MCU 값: " + value + ")");
            }
            catch (Exception ex)
            {
                Log(kind + " 읽기 실패: " + ex.Message);
                MessageBox.Show(this, ex.Message, "읽기 실패", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        /// <summary>이 패널의 유일한 Write 버튼: 화면의 리셋 주기를 RESET_W_ALL로 먼저
        /// 전달하고, 이어서 그 값을 시/분/초로 환산해(<see cref="DecomposePeriod"/>)
        /// _unitKindBox에서 선택된 단위 하나(RTC_W_H/RTC_W_M/RTC_W_S 중 해당하는 것)도
        /// 전송한다. 리셋 주기 쓰기가 실패하면 단위 쓰기는 시도하지 않는다.</summary>
        private async void UnitWriteButton_Click(object sender, EventArgs e)
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
                SavePeriodCache(cfg);
                Log("RESET_W_ALL 완료");
            }
            catch (Exception ex)
            {
                Log("RESET_W_ALL 실패: " + ex.Message);
                MessageBox.Show(this, ex.Message, "쓰기 실패", MessageBoxButtons.OK, MessageBoxIcon.Error);
                return;
            }

            string kind = (string)_unitKindBox.SelectedItem;
            DecomposePeriod(out int hour, out int minute, out int second);
            try
            {
                switch (kind)
                {
                    case UnitKindHour:
                        Log("RTC_W_H 전송... (" + hour + ")");
                        await Stm32Commands.SetRtcHourAsync(SelectedLink, hour, (int)_cmdTimeoutBox.Value);
                        break;
                    case UnitKindMinute:
                        Log("RTC_W_M 전송... (" + minute + ")");
                        await Stm32Commands.SetRtcMinuteAsync(SelectedLink, minute, (int)_cmdTimeoutBox.Value);
                        break;
                    default:
                        Log("RTC_W_S 전송... (" + second + ")");
                        await Stm32Commands.SetRtcSecondAsync(SelectedLink, second, (int)_cmdTimeoutBox.Value);
                        break;
                }
                Log(kind + " 쓰기 완료");
                MessageBox.Show(this, "전달되었습니다.", "RTC 설정", MessageBoxButtons.OK, MessageBoxIcon.Information);
            }
            catch (Exception ex)
            {
                Log(kind + " 쓰기 실패: " + ex.Message);
                MessageBox.Show(this, ex.Message, "쓰기 실패", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }
    }
}
