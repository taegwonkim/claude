using System;
using System.Windows.Forms;
using Stm32WifiConfigTool.Models;
using Stm32WifiConfigTool.Services;

namespace Stm32WifiConfigTool.Panels
{
    /// <summary>
    /// 측정 모듈 설정(Reference/Offset/Resistance/Interval Time) 패널.
    /// "Read"로 MCU에 MEAS_R_ALL을 보내 현재값을 화면에 채우고, "Write"로 입력값 전체를
    /// MEAS_W_ALL 한 프레임에 담아 MCU에 전달한다. "Read" 성공 시 값을 <see cref="AppSettings"/>에
    /// 캐시해두고, 다음 실행 시 <see cref="Initialize"/>가 이를 화면에 미리 채운다(MCU 재조회 전
    /// 참고용).
    /// UI 레이아웃은 <c>MeasurementConfigPanel.Designer.cs</c>에 있으며 Visual Studio 디자이너로 편집 가능하다.
    /// 매개변수 없는 생성자는 디자이너 전용이며, 실제 사용 시에는 생성 직후 <see cref="Initialize"/>를
    /// 호출해 런타임 의존성(ConnectionManager, AppSettings)을 연결해야 한다.
    /// </summary>
    public partial class MeasurementConfigPanel : UserControl
    {
        private ConnectionManager _conn;
        private AppSettings _settings;

        public MeasurementConfigPanel()
        {
            InitializeComponent();
        }

        /// <summary>디자이너가 만든 컨트롤에 실제 동작을 연결한다. MainForm이 생성 직후 1회 호출.</summary>
        public void Initialize(ConnectionManager conn, AppSettings settings)
        {
            _conn = conn;
            _settings = settings;

            bool useUart = settings.MeasConfigCommandChannel == "Uart";
            _channelUsb.Checked = !useUart;
            _channelUart.Checked = useUart;

            _cmdTimeoutBox.Value = ClampDecimal(settings.MeasConfigCommandTimeoutMs, _cmdTimeoutBox.Minimum, _cmdTimeoutBox.Maximum);

            /* 마지막으로 "Read"에 성공했던 값을 화면에 미리 채운다 - MCU를 다시 조회하기 전까지
             * 참고용이며, 실제 값의 원본은 항상 MCU다. */
            ApplyConfigToUi(new MeasurementConfig
            {
                ReferenceMv = settings.MeasReferenceMvCache,
                OffsetMv = settings.MeasOffsetMvCache,
                ResistanceMOhm = settings.MeasResistanceMOhmCache,
                IntervalSec = settings.MeasIntervalSecCache
            });
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
                "Measurement 설정", MessageBoxButtons.OK, MessageBoxIcon.Warning);
            return false;
        }

        private void ApplyConfigToUi(MeasurementConfig cfg)
        {
            _referenceBox.Value = ClampDecimal2(cfg.ReferenceMv, _referenceBox.Minimum, _referenceBox.Maximum);
            _offsetBox.Value = ClampDecimal2(cfg.OffsetMv, _offsetBox.Minimum, _offsetBox.Maximum);
            _resistanceBox.Value = ClampDecimal2(cfg.ResistanceMOhm, _resistanceBox.Minimum, _resistanceBox.Maximum);
            _intervalBox.Value = ClampDecimal2(cfg.IntervalSec, _intervalBox.Minimum, _intervalBox.Maximum);
        }

        private static decimal ClampDecimal2(double value, decimal min, decimal max)
        {
            decimal d = (decimal)value;
            if (d < min) return min;
            if (d > max) return max;
            return d;
        }

        /// <summary>"Read"로 받은 값을 로컬 캐시에 저장하고 즉시 파일에 반영한다(다음 실행 시
        /// <see cref="Initialize"/>가 이 값을 화면에 미리 채운다).</summary>
        private void SaveConfigCache(MeasurementConfig cfg)
        {
            _settings.MeasReferenceMvCache = cfg.ReferenceMv;
            _settings.MeasOffsetMvCache = cfg.OffsetMv;
            _settings.MeasResistanceMOhmCache = cfg.ResistanceMOhm;
            _settings.MeasIntervalSecCache = cfg.IntervalSec;
            try
            {
                AppSettingsStore.Save(_settings);
            }
            catch (Exception)
            {
                /* 설정 저장 실패(권한/디스크 문제 등)로 UI 동작 자체가 막히면 안 되므로 무시 */
            }
        }

        private MeasurementConfig ReadConfigFromUi()
        {
            return new MeasurementConfig
            {
                ReferenceMv = (double)_referenceBox.Value,
                OffsetMv = (double)_offsetBox.Value,
                ResistanceMOhm = (double)_resistanceBox.Value,
                IntervalSec = (double)_intervalBox.Value
            };
        }

        private void ChannelUsb_CheckedChanged(object sender, EventArgs e)
        {
            if (_channelUsb.Checked && _settings != null)
            {
                _settings.MeasConfigCommandChannel = "Usb";
            }
        }

        private void ChannelUart_CheckedChanged(object sender, EventArgs e)
        {
            if (_channelUart.Checked && _settings != null)
            {
                _settings.MeasConfigCommandChannel = "Uart";
            }
        }

        private void CmdTimeoutBox_ValueChanged(object sender, EventArgs e)
        {
            if (_settings != null)
            {
                _settings.MeasConfigCommandTimeoutMs = (int)_cmdTimeoutBox.Value;
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
                Log("MEAS_R_ALL 요청...");
                MeasurementConfig cfg = await Stm32Commands.GetMeasAllAsync(SelectedLink, (int)_cmdTimeoutBox.Value);
                ApplyConfigToUi(cfg);
                SaveConfigCache(cfg);
                Log("MEAS_R_ALL 완료");
            }
            catch (Exception ex)
            {
                Log("MEAS_R_ALL 실패: " + ex.Message);
                MessageBox.Show(this, ex.Message, "읽기 실패", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        private async void WriteButton_Click(object sender, EventArgs e)
        {
            if (!EnsureConnected())
            {
                return;
            }

            MeasurementConfig cfg = ReadConfigFromUi();
            try
            {
                Log("MEAS_W_ALL 전송...");
                await Stm32Commands.SetMeasAllAsync(SelectedLink, cfg, (int)_cmdTimeoutBox.Value);
                Log("MEAS_W_ALL 완료");
                MessageBox.Show(this, "전달되었습니다.", "Measurement 설정", MessageBoxButtons.OK, MessageBoxIcon.Information);
            }
            catch (Exception ex)
            {
                Log("MEAS_W_ALL 실패: " + ex.Message);
                MessageBox.Show(this, ex.Message, "쓰기 실패", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }
    }
}
