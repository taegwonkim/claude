using System;
using System.Windows.Forms;
using Stm32WifiConfigTool.Models;
using Stm32WifiConfigTool.Services;

namespace Stm32WifiConfigTool.Panels
{
    /// <summary>
    /// RTC 리셋 주기(초) 설정 패널. "RESET_R_ALL"/"RESET_W_ALL" 커맨드는 쓰지 않는다 - "리셋 주기"
    /// 값은 항상 "단위"(시/분/초) 콤보박스에서 고른 것에 해당하는 RTC_R_H/RTC_R_M/RTC_R_S(읽기)
    /// 또는 RTC_W_H/RTC_W_M/RTC_W_S(쓰기)로만 주고받는다 - 세 커맨드 모두 값의 의미는 완전히
    /// 같은 "리셋 주기 전체(초)"이며, 어느 것을 쓰든 커맨드 이름만 다를 뿐 결과는 같다(예:
    /// "단위"에서 "시"를 고르면 Read는 RTC_R_H, Write는 RTC_W_H,&lt;리셋 주기 값&gt;을 보낸다).
    /// 값을 시/분/초로 쪼개서 보내지 않는다.
    /// "Read"/"Write" 버튼(<c>_readButton</c>/<c>_writeButton</c>)은 이 패널에 하나씩만 있고,
    /// "단위" 콤보박스에서 선택된 것을 그대로 써서 동작한다(<see cref="UnitReadButton_Click"/>/
    /// <see cref="UnitWriteButton_Click"/>).
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
        private void SavePeriodCache(int periodSec)
        {
            _settings.RtcPeriodSecCache = periodSec;
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

        /// <summary>_unitKindBox에서 선택된 단위 하나에 해당하는 커맨드(RTC_R_H/RTC_R_M/RTC_R_S 중
        /// 하나)로 리셋 주기(초) 전체 값을 조회해 화면에 채우고 캐시한다.</summary>
        private async void UnitReadButton_Click(object sender, EventArgs e)
        {
            if (!EnsureConnected())
            {
                return;
            }

            string kind = (string)_unitKindBox.SelectedItem;
            try
            {
                int periodSec;
                switch (kind)
                {
                    case UnitKindHour:
                        Log("RTC_R_H 요청...");
                        periodSec = await Stm32Commands.GetRtcHourAsync(SelectedLink, (int)_cmdTimeoutBox.Value);
                        break;
                    case UnitKindMinute:
                        Log("RTC_R_M 요청...");
                        periodSec = await Stm32Commands.GetRtcMinuteAsync(SelectedLink, (int)_cmdTimeoutBox.Value);
                        break;
                    default:
                        Log("RTC_R_S 요청...");
                        periodSec = await Stm32Commands.GetRtcSecondAsync(SelectedLink, (int)_cmdTimeoutBox.Value);
                        break;
                }
                _periodBox.Value = ClampDecimal(periodSec, _periodBox.Minimum, _periodBox.Maximum);
                SavePeriodCache(periodSec);
                Log(kind + " 읽기 완료 (리셋 주기: " + periodSec + "초)");
            }
            catch (Exception ex)
            {
                Log(kind + " 읽기 실패: " + ex.Message);
                MessageBox.Show(this, ex.Message, "읽기 실패", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        /// <summary>화면의 "리셋 주기" 값을 _unitKindBox에서 선택된 단위 하나에 해당하는
        /// 커맨드(RTC_W_H/RTC_W_M/RTC_W_S 중 하나)로 그대로 전송한다.</summary>
        private async void UnitWriteButton_Click(object sender, EventArgs e)
        {
            if (!EnsureConnected())
            {
                return;
            }

            string kind = (string)_unitKindBox.SelectedItem;
            int periodSec = (int)_periodBox.Value;
            try
            {
                switch (kind)
                {
                    case UnitKindHour:
                        Log("RTC_W_H 전송... (" + periodSec + ")");
                        await Stm32Commands.SetRtcHourAsync(SelectedLink, periodSec, (int)_cmdTimeoutBox.Value);
                        break;
                    case UnitKindMinute:
                        Log("RTC_W_M 전송... (" + periodSec + ")");
                        await Stm32Commands.SetRtcMinuteAsync(SelectedLink, periodSec, (int)_cmdTimeoutBox.Value);
                        break;
                    default:
                        Log("RTC_W_S 전송... (" + periodSec + ")");
                        await Stm32Commands.SetRtcSecondAsync(SelectedLink, periodSec, (int)_cmdTimeoutBox.Value);
                        break;
                }
                SavePeriodCache(periodSec);
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
