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
    /// FPGA 측정값("&lt;DC IP&gt;,&lt;MAC&gt;,data1,...,data6" 프레임) 표시 패널. USB/UART 채널을
    /// 선택해 어느 쪽(또는 둘 다) 라인을 화면에 표시할지 고를 수 있고, 별도 영역에 WIFI/TCP 관련
    /// EVENT 라인 로그도 보여준다. ESP32 상태(STATUS,&lt;번호&gt;)는 별도 EspStatusPanel에서 표시한다.
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

            if (!Stm32Protocol.TryParseFrame(line, out string[] fields))
            {
                return; /* STX 없는 잡음/깨진 프레임 - 무시 */
            }

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
            else if (Stm32Protocol.IsEventFrame(fields))
            {
                string eventText = string.Join(",", fields);
                _eventLogBox.AppendText(DateTime.Now.ToString("HH:mm:ss.fff") + "  [" + ChannelLabel(channel) + "] " + eventText + Environment.NewLine);
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
