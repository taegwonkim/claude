using System;
using System.Windows.Forms;
using Stm32WifiConfigTool.Models;
using Stm32WifiConfigTool.Services;

namespace Stm32WifiConfigTool.Panels
{
    /// <summary>
    /// RTC 리셋 설정 패널. "리셋 주기"(초)/"단위"(시/분/초)/"리셋 사용"(YES/NO) 세 값을 한
    /// 프레임으로 묶어 RTC_R_ALL(읽기)/RTC_W_ALL(쓰기)로만 주고받는다("RESET_R_ALL"/
    /// "RESET_W_ALL", 그리고 한때 쓰였던 RTC_R_H/M/S·RTC_R_RST 등 개별 커맨드는 더 이상 쓰지
    /// 않는다) - 필드 순서는 "리셋 주기,단위(H/M/S),리셋 사용(YES/NO)"이다(예: 리셋 주기 10초,
    /// 단위 "분", 리셋 사용 "YES"이면 Write는 <c>&lt;STX&gt;RTC_W_ALL,10,M,YES&lt;CR&gt;&lt;LF&gt;</c>를
    /// 보내고, Read는 MCU가 PC로부터 마지막으로 받은(=현재 사용 중인) 값을
    /// <c>&lt;STX&gt;RTC_R_ALL,10,M,YES&lt;CR&gt;&lt;LF&gt;</c> 형태로 그대로 돌려준다).
    /// "단위" 콤보박스는 화면 표시용 한글("시"/"분"/"초")이며, 와이어 프로토콜의 "H"/"M"/"S"
    /// 코드와는 <see cref="UnitKinds"/>/<see cref="UnitCodes"/> 두 배열(순서로 1:1 대응)로
    /// 서로 변환한다.
    /// "Read"/"Write" 버튼(<c>_readButton</c>/<c>_writeButton</c>)은 이 패널에 하나씩만 있고,
    /// 세 값을 항상 함께 읽고 쓴다(<see cref="ReadButton_Click"/>/<see cref="WriteButton_Click"/>).
    /// "Read" 성공 시 세 값을 <see cref="AppSettings"/>에 캐시해두고, 다음 실행 시
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

        /// <summary>"단위" 콤보박스("_unitKindBox")의 항목(화면 표시용 한글) - 순서가 곧 표시
        /// 순서이며, <see cref="UnitCodes"/>와 인덱스로 1:1 대응한다.</summary>
        private static readonly string[] UnitKinds = { UnitKindHour, UnitKindMinute, UnitKindSecond };

        /// <summary>RTC_R_ALL/RTC_W_ALL 와이어 프로토콜의 단위 코드 - <see cref="UnitKinds"/>와
        /// 인덱스로 1:1 대응한다(예: UnitKinds[0]="시" ↔ UnitCodes[0]="H").</summary>
        private static readonly string[] UnitCodes = { "H", "M", "S" };

        private const string YesText = "YES";
        private const string NoText = "NO";

        /// <summary>"리셋 사용" 콤보박스("_resetEnabledBox")의 항목 - 순서가 곧 표시 순서다.</summary>
        private static readonly string[] YesNoOptions = { YesText, NoText };

        public RtcConfigPanel()
        {
            InitializeComponent();
            _unitKindBox.Items.AddRange(UnitKinds);
            _unitKindBox.SelectedIndex = 0;
            _resetEnabledBox.Items.AddRange(YesNoOptions);
            _resetEnabledBox.SelectedIndex = 1; // 기본값: NO
        }

        private void UnitKindBox_SelectedIndexChanged(object sender, EventArgs e)
        {
            if (_settings != null)
            {
                _settings.RtcUnitKindCache = (string)_unitKindBox.SelectedItem;
            }
        }

        private void ResetEnabledBox_SelectedIndexChanged(object sender, EventArgs e)
        {
            if (_settings != null)
            {
                _settings.RtcResetEnabledCache = _resetEnabledBox.SelectedItem as string == YesText;
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

            /* 마지막으로 "Read"에 성공했던 값들을 화면에 미리 채운다 - MCU를 다시 조회하기
             * 전까지 참고용이며, 실제 값의 원본은 항상 MCU다. */
            _periodBox.Value = ClampDecimal(settings.RtcPeriodSecCache, _periodBox.Minimum, _periodBox.Maximum);

            /* 마지막으로 선택했던 단위(시/분/초)를 미리 고른다 - 목록에 없는 값이 저장돼 있으면
             * (예: 설정 파일 손상) 첫 항목("시")으로 대체한다. */
            int kindIndex = Array.IndexOf(UnitKinds, settings.RtcUnitKindCache);
            _unitKindBox.SelectedIndex = kindIndex >= 0 ? kindIndex : 0;

            /* 마지막으로 "Read"에 성공했던 "리셋 사용" 값을 미리 채운다(참고용, 원본은 MCU). */
            _resetEnabledBox.SelectedIndex = settings.RtcResetEnabledCache ? 0 : 1;
        }

        private static decimal ClampDecimal(int value, decimal min, decimal max)
        {
            if (value < min) return min;
            if (value > max) return max;
            return value;
        }

        private SerialLinkService SelectedLink => _channelUsb.Checked ? _conn.Usb : _conn.Uart;

        /// <summary>"Read"/"Write"로 주고받은 리셋 주기/단위/리셋 사용 값을 로컬 캐시에 저장하고
        /// 즉시 파일에 반영한다(다음 실행 시 <see cref="Initialize"/>가 이 값을 화면에 미리
        /// 채운다).</summary>
        private void SaveRtcAllCache(int periodSec, string unitKindText, bool resetEnabled)
        {
            _settings.RtcPeriodSecCache = periodSec;
            _settings.RtcUnitKindCache = unitKindText;
            _settings.RtcResetEnabledCache = resetEnabled;
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

        /// <summary>RTC_R_ALL을 보내 리셋 주기/단위/리셋 사용 값을 한 번에 조회해 세 컨트롤
        /// (<c>_periodBox</c>/<c>_unitKindBox</c>/<c>_resetEnabledBox</c>)을 모두 응답값에 맞춰
        /// 채우고 캐시한다.</summary>
        private async void ReadButton_Click(object sender, EventArgs e)
        {
            if (!EnsureConnected())
            {
                return;
            }

            try
            {
                Log("RTC_R_ALL 요청...");
                RtcAllConfig cfg = await Stm32Commands.GetRtcAllAsync(SelectedLink, (int)_cmdTimeoutBox.Value);

                _periodBox.Value = ClampDecimal(cfg.PeriodSec, _periodBox.Minimum, _periodBox.Maximum);

                int kindIndex = Array.IndexOf(UnitCodes, cfg.UnitCode);
                _unitKindBox.SelectedIndex = kindIndex >= 0 ? kindIndex : 0;
                string unitKindText = (string)_unitKindBox.SelectedItem;

                _resetEnabledBox.SelectedIndex = cfg.ResetEnabled ? 0 : 1;

                SaveRtcAllCache(cfg.PeriodSec, unitKindText, cfg.ResetEnabled);
                Log("RTC_R_ALL 읽기 완료 (리셋 주기: " + cfg.PeriodSec + "초, 단위: " + unitKindText +
                    ", 리셋 사용: " + (cfg.ResetEnabled ? YesText : NoText) + ")");
            }
            catch (Exception ex)
            {
                Log("RTC_R_ALL 읽기 실패: " + ex.Message);
                MessageBox.Show(this, ex.Message, "읽기 실패", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        /// <summary>화면의 리셋 주기/단위/리셋 사용 값을 RTC_W_ALL 한 프레임으로 그대로
        /// 전송한다.</summary>
        private async void WriteButton_Click(object sender, EventArgs e)
        {
            if (!EnsureConnected())
            {
                return;
            }

            int periodSec = (int)_periodBox.Value;
            string unitKindText = (string)_unitKindBox.SelectedItem;
            string unitCode = UnitCodes[_unitKindBox.SelectedIndex];
            bool enabled = (string)_resetEnabledBox.SelectedItem == YesText;

            var cfg = new RtcAllConfig
            {
                PeriodSec = periodSec,
                UnitCode = unitCode,
                ResetEnabled = enabled
            };

            try
            {
                Log("RTC_W_ALL 전송... (" + periodSec + "," + unitCode + "," + (enabled ? YesText : NoText) + ")");
                await Stm32Commands.SetRtcAllAsync(SelectedLink, cfg, (int)_cmdTimeoutBox.Value);
                SaveRtcAllCache(periodSec, unitKindText, enabled);
                Log("RTC_W_ALL 쓰기 완료");
                MessageBox.Show(this, "전달되었습니다.", "RTC 설정", MessageBoxButtons.OK, MessageBoxIcon.Information);
            }
            catch (Exception ex)
            {
                Log("RTC_W_ALL 쓰기 실패: " + ex.Message);
                MessageBox.Show(this, ex.Message, "쓰기 실패", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }
    }
}
