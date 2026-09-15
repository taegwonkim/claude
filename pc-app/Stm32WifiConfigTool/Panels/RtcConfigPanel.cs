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
    /// (2) "시/분/초 개별 설정" - 값을 직접 입력하지 않고 콤보박스로 고른다: "단위" 콤보박스에서
    /// 시/분/초 중 하나를 고르면, "값" 콤보박스가 그 단위에 맞는 범위(시=0~99, 분/초=0~59)로
    /// 다시 채워진다(<see cref="UnitKindBox_SelectedIndexChanged"/>). Read/Write는 그 순간
    /// 선택된 단위 하나에 대해서만 RTC_R_H/RTC_R_M/RTC_R_S 또는 RTC_W_H/RTC_W_M/RTC_W_S 중
    /// 해당하는 한 프레임만 보낸다. (1)과는 완전히 별도의 값이며 자체 Read/Write 버튼
    /// (<c>_unitReadButton</c>/<c>_unitWriteButton</c>)을 따로 쓴다 - 채널 선택/커맨드
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

        private const string UnitKindHour = "시";
        private const string UnitKindMinute = "분";
        private const string UnitKindSecond = "초";

        /// <summary>"단위" 콤보박스("_unitKindBox")의 항목 - 순서가 곧 표시 순서다.</summary>
        private static readonly string[] UnitKinds = { UnitKindHour, UnitKindMinute, UnitKindSecond };

        /// <summary>"값" 콤보박스에 채워 넣는 값의 최댓값(0부터 이 값까지) - 단위가 "분"/"초"일 때.</summary>
        private const int MinuteSecondComboMax = 59;

        /// <summary>"값" 콤보박스에 채워 넣는 값의 최댓값(0부터 이 값까지) - 단위가 "시"일 때.</summary>
        private const int HourComboMax = 99;

        public RtcConfigPanel()
        {
            InitializeComponent();
            _unitKindBox.Items.AddRange(UnitKinds);
            _unitKindBox.SelectedIndex = 0; /* SelectedIndexChanged가 발생해 _unitValueBox도 채워진다(이 시점엔 _settings가 아직 null이라 캐시값 대신 0이 선택됨 - Initialize()가 실제 캐시값으로 다시 채운다). */
        }

        /// <summary>kind("시"/"분"/"초")에 해당하는 "값" 콤보박스의 최댓값을 반환한다.</summary>
        private static int MaxForKind(string kind)
        {
            return kind == UnitKindHour ? HourComboMax : MinuteSecondComboMax;
        }

        /// <summary>kind에 해당하는 마지막 캐시값을 반환한다("Read"에 성공한 적 없으면 0).</summary>
        private int GetCachedUnitValue(string kind)
        {
            if (_settings == null)
            {
                return 0;
            }
            switch (kind)
            {
                case UnitKindHour: return _settings.RtcHourCache;
                case UnitKindMinute: return _settings.RtcMinuteCache;
                default: return _settings.RtcSecondCache;
            }
        }

        /// <summary>_unitKindBox에서 선택된 단위에 맞춰 _unitValueBox의 항목(0~해당 최댓값)을 다시
        /// 채우고, 그 단위의 캐시값(또는 0)을 기본 선택한다.</summary>
        private void PopulateUnitValueCombo()
        {
            string kind = (string)_unitKindBox.SelectedItem;
            int max = MaxForKind(kind);

            _unitValueBox.Items.Clear();
            for (int i = 0; i <= max; i++)
            {
                _unitValueBox.Items.Add(i);
            }
            _unitValueBox.SelectedIndex = ClampIndex(GetCachedUnitValue(kind), max);
        }

        private void UnitKindBox_SelectedIndexChanged(object sender, EventArgs e)
        {
            PopulateUnitValueCombo();
            if (_settings != null)
            {
                _settings.RtcUnitKindCache = (string)_unitKindBox.SelectedItem;
            }
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

            /* 마지막으로 선택했던 단위(시/분/초)를 미리 고른다 - 목록에 없는 값이 저장돼 있으면
             * (예: 설정 파일 손상) 첫 항목("시")으로 대체한다. SelectedIndex가 생성자에서 이미
             * 0으로 설정돼 있어 대입해도 값이 같으면 SelectedIndexChanged가 발생하지 않을 수
             * 있으므로, PopulateUnitValueCombo()를 직접 한 번 더 호출해 이제는 값을 아는
             * _settings 기준으로 _unitValueBox를 확실히 다시 채운다. */
            int kindIndex = Array.IndexOf(UnitKinds, settings.RtcUnitKindCache);
            _unitKindBox.SelectedIndex = kindIndex >= 0 ? kindIndex : 0;
            PopulateUnitValueCombo();
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

        /// <summary>"Read"(시/분/초 중 하나)로 받은 값을 kind에 해당하는 캐시 필드에 저장하고
        /// 즉시 파일에 반영한다(다음 실행 시 <see cref="Initialize"/>가 이 값을 화면에 미리 채운다).</summary>
        private void SaveUnitCache(string kind, int value)
        {
            switch (kind)
            {
                case UnitKindHour: _settings.RtcHourCache = value; break;
                case UnitKindMinute: _settings.RtcMinuteCache = value; break;
                default: _settings.RtcSecondCache = value; break;
            }
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

        /// <summary>_unitKindBox에서 선택된 단위 하나에 대해서만 RTC_R_H/RTC_R_M/RTC_R_S 중
        /// 해당하는 한 프레임을 보내 값을 조회해 화면에 채운다(위 "리셋 주기"와는 완전히 별도의 값).</summary>
        private async void UnitReadButton_Click(object sender, EventArgs e)
        {
            if (!EnsureConnected())
            {
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
                _unitValueBox.SelectedIndex = ClampIndex(value, MaxForKind(kind));
                SaveUnitCache(kind, value);
                Log(kind + " 읽기 완료 (" + value + ")");
            }
            catch (Exception ex)
            {
                Log(kind + " 읽기 실패: " + ex.Message);
                MessageBox.Show(this, ex.Message, "읽기 실패", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        /// <summary>_unitKindBox에서 선택된 단위 하나에 대해서만 화면의 "값"을 RTC_W_H/RTC_W_M/
        /// RTC_W_S 중 해당하는 한 프레임으로 MCU에 전달한다.</summary>
        private async void UnitWriteButton_Click(object sender, EventArgs e)
        {
            if (!EnsureConnected())
            {
                return;
            }

            string kind = (string)_unitKindBox.SelectedItem;
            int value = _unitValueBox.SelectedIndex;
            try
            {
                switch (kind)
                {
                    case UnitKindHour:
                        Log("RTC_W_H 전송... (" + value + ")");
                        await Stm32Commands.SetRtcHourAsync(SelectedLink, value, (int)_cmdTimeoutBox.Value);
                        break;
                    case UnitKindMinute:
                        Log("RTC_W_M 전송... (" + value + ")");
                        await Stm32Commands.SetRtcMinuteAsync(SelectedLink, value, (int)_cmdTimeoutBox.Value);
                        break;
                    default:
                        Log("RTC_W_S 전송... (" + value + ")");
                        await Stm32Commands.SetRtcSecondAsync(SelectedLink, value, (int)_cmdTimeoutBox.Value);
                        break;
                }
                SaveUnitCache(kind, value);
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
