using System;
using System.ComponentModel;
using System.Drawing;
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
    /// MainForm에 다른 패널들과 함께 한 창에 도킹되어 표시된다.
    /// </summary>
    public class MeasurementPanel : UserControl
    {
        private const int MaxRows = 5000; // 메모리 보호용 상한, 초과 시 오래된 행부터 제거

        private readonly ConnectionManager _conn;
        private readonly BindingList<MeasurementRecord> _records = new BindingList<MeasurementRecord>();

        private readonly RadioButton _showUsb;
        private readonly RadioButton _showUart;
        private readonly RadioButton _showBoth;
        private readonly DataGridView _grid;
        private readonly TextBox _eventLogBox;
        private readonly Label _countLabel;
        private readonly CheckBox _autoScrollCheck;

        public MeasurementPanel(ConnectionManager conn, AppSettings settings)
        {
            _conn = conn;

            var root = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 1, Padding = new Padding(6) };
            root.RowStyles.Add(new RowStyle(SizeType.AutoSize));
            root.RowStyles.Add(new RowStyle(SizeType.Percent, 70));
            root.RowStyles.Add(new RowStyle(SizeType.AutoSize));
            root.RowStyles.Add(new RowStyle(SizeType.Percent, 30));

            // 상단: 채널 선택 + 도구 버튼
            var topRow = new FlowLayoutPanel { Dock = DockStyle.Top, AutoSize = true, WrapContents = false };
            var channelGroup = new GroupBox { Text = "표시 채널", AutoSize = true, Height = 50, Width = 220 };
            _showUsb = new RadioButton { Text = "USB", Left = 10, Top = 20, AutoSize = true, Checked = settings.MeasurementDisplayChannel == "Usb" };
            _showUart = new RadioButton { Text = "UART", Left = 70, Top = 20, AutoSize = true, Checked = settings.MeasurementDisplayChannel == "Uart" };
            _showBoth = new RadioButton { Text = "둘 다", Left = 140, Top = 20, AutoSize = true, Checked = settings.MeasurementDisplayChannel != "Usb" && settings.MeasurementDisplayChannel != "Uart" };
            _showUsb.CheckedChanged += (s, e) => { if (_showUsb.Checked) settings.MeasurementDisplayChannel = "Usb"; };
            _showUart.CheckedChanged += (s, e) => { if (_showUart.Checked) settings.MeasurementDisplayChannel = "Uart"; };
            _showBoth.CheckedChanged += (s, e) => { if (_showBoth.Checked) settings.MeasurementDisplayChannel = "Both"; };
            channelGroup.Controls.Add(_showUsb);
            channelGroup.Controls.Add(_showUart);
            channelGroup.Controls.Add(_showBoth);
            topRow.Controls.Add(channelGroup);

            var clearButton = new Button { Text = "지우기", AutoSize = true, Margin = new Padding(10, 15, 3, 3) };
            clearButton.Click += ClearButton_Click;
            topRow.Controls.Add(clearButton);

            var exportButton = new Button { Text = "CSV로 저장", AutoSize = true, Margin = new Padding(3, 15, 3, 3) };
            exportButton.Click += ExportButton_Click;
            topRow.Controls.Add(exportButton);

            _autoScrollCheck = new CheckBox { Text = "자동 스크롤", AutoSize = true, Checked = settings.MeasurementAutoScroll, Margin = new Padding(10, 20, 3, 3) };
            _autoScrollCheck.CheckedChanged += (s, e) => settings.MeasurementAutoScroll = _autoScrollCheck.Checked;
            topRow.Controls.Add(_autoScrollCheck);

            root.Controls.Add(topRow, 0, 0);

            // 측정값 그리드
            _grid = new DataGridView
            {
                Dock = DockStyle.Fill,
                ReadOnly = true,
                AllowUserToAddRows = false,
                AllowUserToDeleteRows = false,
                AutoGenerateColumns = false,
                SelectionMode = DataGridViewSelectionMode.FullRowSelect,
                RowHeadersVisible = false,
                DataSource = _records
            };
            _grid.Columns.Add(new DataGridViewTextBoxColumn { HeaderText = "수신 시각", DataPropertyName = "ReceivedAt", Width = 140, DefaultCellStyle = new DataGridViewCellStyle { Format = "HH:mm:ss.fff" } });
            _grid.Columns.Add(new DataGridViewTextBoxColumn { HeaderText = "채널", DataPropertyName = "SourceChannel", Width = 60 });
            _grid.Columns.Add(new DataGridViewTextBoxColumn { HeaderText = "DC IP", DataPropertyName = "DcIp", Width = 110 });
            _grid.Columns.Add(new DataGridViewTextBoxColumn { HeaderText = "MAC", DataPropertyName = "MacAddress", Width = 130 });
            _grid.Columns.Add(new DataGridViewTextBoxColumn { HeaderText = "Data1..6", DataPropertyName = "SamplesText", Width = 260, AutoSizeMode = DataGridViewAutoSizeColumnMode.Fill });
            root.Controls.Add(_grid, 0, 1);

            var eventLabel = new Label { Text = "이벤트 로그 (WIFI/TCP 상태 등)", Dock = DockStyle.Top, AutoSize = true, Padding = new Padding(0, 6, 0, 2) };
            root.Controls.Add(eventLabel, 0, 2);

            _eventLogBox = new TextBox
            {
                Dock = DockStyle.Fill,
                Multiline = true,
                ReadOnly = true,
                ScrollBars = ScrollBars.Vertical,
                Font = new Font(FontFamily.GenericMonospace, 8.5f)
            };
            root.Controls.Add(_eventLogBox, 0, 3);

            _countLabel = new Label { Text = "0건", Dock = DockStyle.Bottom, TextAlign = ContentAlignment.MiddleRight };
            Controls.Add(_countLabel);

            Controls.Add(root);

            _conn.Usb.LineReceived += OnLineReceived;
            _conn.Uart.LineReceived += OnLineReceived;
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
                _conn.Usb.LineReceived -= OnLineReceived;
                _conn.Uart.LineReceived -= OnLineReceived;
            }
            base.Dispose(disposing);
        }
    }
}
