using System;
using System.ComponentModel;
using System.IO;
using System.Text;
using System.Windows.Forms;
using Stm32WifiConfigTool.Models;
using Stm32WifiConfigTool.Services;

namespace Stm32WifiConfigTool.Panels
{
    /// <summary>
    /// FPGA 측정값("DC_&lt;dc_ip&gt;,&lt;mac&gt;,data1,...,dataN" 프레임, 첫 필드 "DC_" 접두어로 식별,
    /// 샘플 개수는 고정이 아님) 표시 패널. USB/UART 채널을
    /// 선택해 어느 쪽(또는 둘 다) 라인을 화면에 표시할지 고를 수 있다. 화면은 좌/우로 나뉘어
    /// 있다: 좌측은 측정값 그리드, 우측은 그 외 모든 프레임(EVENT/RESET_COUNT/커맨드 응답/STATUS
    /// 등)을 한 로그에 원본 그대로 모아 보여준다 — ESP32 상태(STATUS)는 별도 EspStatusPanel의
    /// "현재 ESP32 상태"에 이미 크게 표시되므로 여기서는 별도 칸을 두지 않는다. 측정값이 아닌
    /// 프레임은 STX 유무에 관계없이 전부 표시된다(<see cref="Stm32Protocol.DisplayText"/> 참고).
    /// UI 레이아웃은 <c>MeasurementPanel.Designer.cs</c>에 있으며 Visual Studio 디자이너로 편집
    /// 가능하다. 매개변수 없는 생성자는 디자이너 전용이며, 실제 사용 시에는 생성 직후
    /// <see cref="Initialize"/>를 호출해 런타임 의존성(ConnectionManager, AppSettings)을 연결해야 한다.
    /// MainForm에 다른 패널들과 함께 한 창에 도킹되어 표시된다.
    /// </summary>
    public partial class MeasurementPanel : UserControl
    {
        private const int MaxRows = 5000; // 메모리 보호용 상한, 초과 시 오래된 행부터 제거

        private readonly BindingList<MeasurementRecord> _records = new BindingList<MeasurementRecord>();
        private ConnectionManager _conn;
        private AppSettings _settings;

        public MeasurementPanel()
        {
            InitializeComponent();
            _grid.DataSource = _records;
        }

        /// <summary>디자이너가 만든 컨트롤에 실제 동작을 연결한다. MainForm이 생성 직후 1회 호출.</summary>
        public void Initialize(ConnectionManager conn, AppSettings settings)
        {
            _conn = conn;
            _settings = settings;

            _showUsb.Checked = settings.MeasurementDisplayChannel == "Usb";
            _showUart.Checked = settings.MeasurementDisplayChannel == "Uart";
            _showBoth.Checked = settings.MeasurementDisplayChannel != "Usb" && settings.MeasurementDisplayChannel != "Uart";
            _autoScrollCheck.Checked = settings.MeasurementAutoScroll;

            _conn.Usb.LineReceived += OnLineReceived;
            _conn.Uart.LineReceived += OnLineReceived;

            /* MainForm의 상단 4개 스플리터와 동일한 이유로 BeginInvoke를 통해 지연 복원한다:
             * 생성 직후에는 SplitContainer의 Width가 아직 최종값으로 안정되지 않을 수 있다. */
            Load += (s, e) => BeginInvoke(new Action(ApplySavedSplitterDistance));
        }

        private void ApplySavedSplitterDistance()
        {
            int min = _splitDisplay.Panel1MinSize;
            int max = _splitDisplay.Width - _splitDisplay.Panel2MinSize - _splitDisplay.SplitterWidth;
            if (max < min)
            {
                return; /* 아직 폭이 좁아 유효 범위를 계산할 수 없음 - 디자이너 기본값 유지 */
            }
            _splitDisplay.SplitterDistance = Math.Max(min, Math.Min(max, _settings.MeasurementGridWidth));
        }

        private void SplitDisplay_SplitterMoved(object sender, SplitterEventArgs e)
        {
            if (_settings == null)
            {
                return;
            }
            _settings.MeasurementGridWidth = _splitDisplay.SplitterDistance;
            try
            {
                AppSettingsStore.Save(_settings);
            }
            catch (Exception)
            {
                /* 설정 저장 실패(권한/디스크 문제 등)로 UI 동작 자체가 막히면 안 되므로 무시 */
            }
        }

        private void ShowUsb_CheckedChanged(object sender, EventArgs e)
        {
            if (_showUsb.Checked && _settings != null)
            {
                _settings.MeasurementDisplayChannel = "Usb";
            }
        }

        private void ShowUart_CheckedChanged(object sender, EventArgs e)
        {
            if (_showUart.Checked && _settings != null)
            {
                _settings.MeasurementDisplayChannel = "Uart";
            }
        }

        private void ShowBoth_CheckedChanged(object sender, EventArgs e)
        {
            if (_showBoth.Checked && _settings != null)
            {
                _settings.MeasurementDisplayChannel = "Both";
            }
        }

        private void AutoScrollCheck_CheckedChanged(object sender, EventArgs e)
        {
            if (_settings != null)
            {
                _settings.MeasurementAutoScroll = _autoScrollCheck.Checked;
            }
        }

        private bool IsChannelSelected(LinkChannel channel)
        {
            if (_showBoth.Checked)
            {
                return true;
            }
            return (channel == LinkChannel.Usb && _showUsb.Checked) || (channel == LinkChannel.Uart && _showUart.Checked);
        }

        private static string ChannelLabel(LinkChannel channel) => channel == LinkChannel.Usb ? "USB" : "UART";

        // SerialLinkService.LineReceived는 백그라운드 읽기 스레드에서 호출되므로 반드시 UI 스레드로 마샬링한다.
        private void OnLineReceived(LinkChannel channel, string line)
        {
            if (IsDisposed || !IsHandleCreated)
            {
                return;
            }

            try
            {
                BeginInvoke(new Action(() => HandleLineOnUiThread(channel, line)));
            }
            catch (ObjectDisposedException)
            {
                /* 폼이 닫히는 중 - 무시 */
            }
        }

        private void HandleLineOnUiThread(LinkChannel channel, string line)
        {
            if (!IsChannelSelected(channel))
            {
                return;
            }

            /* STX 유무와 관계없이 처리한다(실측 결과 MCU가 모든 프레임에 STX를 붙이지는 않음) -
             * DisplayText는 STX가 있으면 떼고, 없으면 원본 그대로 돌려준다. */
            string payload = Stm32Protocol.DisplayText(line);
            if (payload.Length == 0)
            {
                return; /* 빈 줄 - 무시 */
            }
            string[] fields = payload.Split(',');

            if (Stm32Protocol.TryParseMeasurementRecord(fields, ChannelLabel(channel), out MeasurementRecord record))
            {
                _records.Add(record);
                while (_records.Count > MaxRows)
                {
                    _records.RemoveAt(0);
                }
                _countLabel.Text = _records.Count + "건";

                if (_autoScrollCheck.Checked && _grid.Rows.Count > 0)
                {
                    _grid.FirstDisplayedScrollingRowIndex = _grid.Rows.Count - 1;
                }
            }
            else
            {
                /* 측정값이 아닌 나머지 전부(STATUS/EVENT/RESET_COUNT/커맨드 응답 및 STX 없이 오는
                 * 값 포함)는 우측 일반 로그에 원본 그대로 표시한다. */
                _eventLogBox.AppendText(DateTime.Now.ToString("HH:mm:ss.fff") + "  [" + ChannelLabel(channel) + "] " + payload + Environment.NewLine);
            }
        }

        private void ClearButton_Click(object sender, EventArgs e)
        {
            _records.Clear();
            _countLabel.Text = "0건";
            _eventLogBox.Clear();
        }

        private void ExportButton_Click(object sender, EventArgs e)
        {
            if (_records.Count == 0)
            {
                MessageBox.Show(this, "저장할 측정값이 없습니다.", "CSV로 저장", MessageBoxButtons.OK, MessageBoxIcon.Information);
                return;
            }

            using (var dialog = new SaveFileDialog { Filter = "CSV 파일|*.csv", FileName = "measurements_" + DateTime.Now.ToString("yyyyMMdd_HHmmss") + ".csv" })
            {
                if (dialog.ShowDialog(this) != DialogResult.OK)
                {
                    return;
                }

                try
                {
                    using (var writer = new StreamWriter(dialog.FileName, false, Encoding.UTF8))
                    {
                        writer.WriteLine("ReceivedAt,Channel,DcIp,MacAddress,Samples");
                        foreach (MeasurementRecord r in _records)
                        {
                            writer.WriteLine(
                                r.ReceivedAt.ToString("yyyy-MM-dd HH:mm:ss.fff") + "," +
                                r.SourceChannel + "," +
                                r.DcIp + "," +
                                r.MacAddress + "," +
                                "\"" + r.SamplesText + "\"");
                        }
                    }
                    MessageBox.Show(this, "저장되었습니다:\n" + dialog.FileName, "CSV로 저장", MessageBoxButtons.OK, MessageBoxIcon.Information);
                }
                catch (Exception ex)
                {
                    MessageBox.Show(this, "저장 실패: " + ex.Message, "CSV로 저장", MessageBoxButtons.OK, MessageBoxIcon.Error);
                }
            }
        }

        protected override void Dispose(bool disposing)
        {
            if (disposing)
            {
                if (_conn != null)
                {
                    _conn.Usb.LineReceived -= OnLineReceived;
                    _conn.Uart.LineReceived -= OnLineReceived;
                }
                components?.Dispose();
            }
            base.Dispose(disposing);
        }
    }
}
