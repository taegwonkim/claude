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
    /// 선택해 어느 쪽 라인을 화면에 표시할지 고를 수 있다. 화면은 좌/우로 나뉘어
    /// 있다: 좌측은 측정값 그리드, 우측은 그 외 모든 프레임(EVENT/RESET_COUNT/커맨드 응답/STATUS
    /// 등)을 한 로그에 원본 그대로 모아 보여준다 — ESP32 상태(STATUS)는 별도 EspStatusPanel의
    /// "현재 ESP32 상태"에 이미 크게 표시되므로 여기서는 별도 칸을 두지 않는다. 측정값이 아닌
    /// 프레임은 STX 유무에 관계없이 전부 표시된다(<see cref="Stm32Protocol.DisplayText"/> 참고).
    /// 그 중 "MAC_&lt;mac address&gt;" 형식("&lt;STX&gt;MAC_mac address&lt;CR&gt;&lt;LF&gt;"로 옴,
    /// <see cref="Stm32Protocol.TryParseMacAddress"/> 참고)인 경우에는 원본 프레임을 위 로그에
    /// 그대로 남기는 것과 별도로, "MAC_" 뒤의 값만 상단 "자동 스크롤" 체크박스 옆의
    /// MAC Address 표시 영역에도 갱신한다. 이 캡션 라벨(<c>_macAddressCaptionLabel</c>)과
    /// 값 표시 텍스트박스(<c>_macAddressValueLabel</c>, 한 줄짜리 single-line TextBox)는
    /// <c>_macAddressGroup</c>이라는 "자유 배치" 컨테이너(Panel) 안에 들어 있어서, 둘 다
    /// Visual Studio 디자이너에서 폭/높이/위치를 완전히 자유롭게 조절할 수 있다 -
    /// <c>_topRow</c>(FlowLayoutPanel)가 자동으로 다시 배치하는 것은 이 컨테이너
    /// (<c>_macAddressGroup</c>) 자체의 위치일 뿐이고, 그 안에 있는 두 컨트롤은 그 영향을
    /// 받지 않는다(<c>MeasurementPanel.Designer.cs</c>의 <c>_macAddressGroup</c> 주석 참고).
    /// <c>_topRow</c> 자체의 높이는 <c>_root</c>(TableLayoutPanel)의 1행이 고정 높이(Absolute
    /// 64px)로 못박혀 있어서, 이 안(특히 <c>_macAddressGroup</c>)의 컨트롤을 아무리 늘리거나
    /// 옮겨도 아래쪽 측정값 그리드/로그 영역(<c>_splitDisplay</c>, 2행 Percent 100%)의 크기에는
    /// 전혀 영향을 주지 않는다 - 예전에는 1행이 auto-size라 위쪽을 키우면 그만큼 아래쪽이
    /// 줄어드는 부작용이 있었다(<c>MeasurementPanel.Designer.cs</c>의 <c>_root</c> 주석 참고).
    /// 이 행 높이 고정은 Visual Studio 디자이너가 <c>InitializeComponent()</c>를 다시 쓸 때
    /// 조용히 원래 상태(auto-size)로 되돌아간 사례가 있어서, 생성자(디자이너가 건드리지 않는
    /// 곳)에서 한 번 더 강제로 재적용한다. 측정값 그리드의 열(<c>_colTimeStamp</c> 등)도 같은
    /// 이유로 <c>Designer.cs</c>가 아니라 <see cref="BuildGridColumns"/>에서 코드로 직접
    /// 만든다 - Designer.cs에 있을 때는 이 열 정의 자체가 통째로 유실되어 앱 시작이 막히는
    /// 사고가 있었다.
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

        /* 측정값 그리드의 열들 - Visual Studio 디자이너가 InitializeComponent()를 다시 쓸 때
         * DataGridView의 열 정의(_colTimeStamp, 그다음 _colSamples)가 통째로 유실되어
         * NullReferenceException으로 앱 시작이 막힌 사고가 두 번 있었다. 그래서 이 열들은
         * Designer.cs가 아니라 여기(디자이너가 절대 건드리지 않는 코드 비하인드)에서
         * BuildGridColumns()로 직접 만든다 - 이제 이 필드들이 null이 되는 경우는 원천적으로
         * 없다. */
        private DataGridViewTextBoxColumn _colTimeStamp;
        private DataGridViewTextBoxColumn _colDcIp;
        private DataGridViewTextBoxColumn _colMac;
        private DataGridViewTextBoxColumn _colSamples;
        private DataGridViewTextBoxColumn _colRawLine;

        public MeasurementPanel()
        {
            InitializeComponent();
            BuildGridColumns();
            _grid.DataSource = _records;

            /* Visual Studio 디자이너에서 MeasurementPanel을 열고 아무 속성이나(예: MAC Address
             * 라벨/텍스트박스의 크기나 위치) 바꿔 저장하면 InitializeComponent() 전체가 다시
             * 생성되는데, 이 파일이 손으로 작성된 탓에 완전한 라운드트립이 보장되지 않아 _root
             * 1행 높이가 다시 auto-size로 바뀌어 MAC Address 쪽을 조절하면 아래 측정값
             * 그리드/로그 영역이 줄어드는 문제가 실제로 있었다. 디자이너가 InitializeComponent()에
             * 무엇을 써놓든, 이 생성자(디자이너가 건드리지 않는 곳)에서 마지막에 다시 한번
             * 강제로 맞춰 두면 항상 고정 높이가 보장된다. */
            if (_root != null && _root.RowStyles.Count >= 2)
            {
                _root.RowStyles[0] = new RowStyle(SizeType.Absolute, 64F);
                _root.RowStyles[1] = new RowStyle(SizeType.Percent, 100F);
            }
            if (_topRow != null)
            {
                _topRow.AutoSize = false;
            }
        }

        /// <summary>측정값 그리드의 열을 만들어 _grid에 연결한다 - 위 필드 주석 참고. TimeStamp는
        /// "HH:mm:ss:fff" 형식(밀리초 앞도 콜론)으로 표시하고, RawLine은 항상 마지막 열에 놓고
        /// 남는 폭을 모두 채운다(AutoSizeMode.Fill). CSV 내보내기(ExportButton_Click)는 이
        /// _grid.Columns를 그대로 따르므로, 여기 있는 열 구성/순서가 CSV에도 똑같이 반영된다.
        /// AllowUserToOrderColumns를 켜서 열 머리글을 드래그해 순서를 바꿀 수 있게 한다(폭과
        /// 달리 순서 자체는 저장/복원하지 않는다).</summary>
        private void BuildGridColumns()
        {
            _colTimeStamp = new DataGridViewTextBoxColumn
            {
                DataPropertyName = "TimeStamp",
                DefaultCellStyle = new DataGridViewCellStyle { Format = "HH:mm:ss:fff" },
                HeaderText = "TimeStamp",
                Name = "_colTimeStamp",
                ReadOnly = true,
                Width = 140
            };
            _colDcIp = new DataGridViewTextBoxColumn
            {
                DataPropertyName = "DcIp",
                HeaderText = "DC IP",
                Name = "_colDcIp",
                ReadOnly = true,
                Width = 110
            };
            _colMac = new DataGridViewTextBoxColumn
            {
                DataPropertyName = "MacAddress",
                HeaderText = "MAC",
                Name = "_colMac",
                ReadOnly = true,
                Width = 130
            };
            _colSamples = new DataGridViewTextBoxColumn
            {
                DataPropertyName = "SamplesText",
                HeaderText = "Data1..N",
                Name = "_colSamples",
                ReadOnly = true,
                Width = 260
            };
            _colRawLine = new DataGridViewTextBoxColumn
            {
                AutoSizeMode = DataGridViewAutoSizeColumnMode.Fill,
                DataPropertyName = "RawLine",
                HeaderText = "RawLine",
                MinimumWidth = 150,
                Name = "_colRawLine",
                ReadOnly = true
            };

            _grid.Columns.AddRange(_colTimeStamp, _colDcIp, _colMac, _colSamples, _colRawLine);
            _grid.AllowUserToOrderColumns = true;

            /* AutoGenerateColumns가 true면 DataSource(_records, MeasurementRecord 목록)에
             * 바인딩되는 순간 위 5개 열 외에 MeasurementRecord의 나머지 공개 프로퍼티
             * (SourceChannel/Samples 등, 화면에 두지 않기로 한 것들)까지 전부 열로 자동
             * 생성되어 "SourceChannel" 같은 열이 다시 나타난다. Designer.cs에도 이미
             * AutoGenerateColumns = false가 있지만, 디자이너가 InitializeComponent()를 다시
             * 쓸 때 이 값이 되돌아갈 수 있다는 게 이미 여러 번 확인된 터라, 디자이너가 건드리지
             * 않는 여기서도 다시 한번 false로 못박아 둔다. */
            _grid.AutoGenerateColumns = false;
        }

        /// <summary>디자이너가 만든 컨트롤에 실제 동작을 연결한다. MainForm이 생성 직후 1회 호출.
        /// _showUsb/_showUart/_autoScrollCheck는 InitializeComponent()가 만든 것인데, 이 파일이
        /// 손으로 작성된 탓에 Visual Studio 디자이너가 InitializeComponent()를 다시 쓸 때 특정
        /// 컨트롤의 생성 코드가 유실되는 사고가 실제로 있었다. 그런 손상이 다시 있어도 앱 전체가
        /// 죽지 않도록, SafeSetChecked로 감싸 null이면 그 항목만 조용히 건너뛴다 - 화면 일부가
        /// 설정을 못 불러올 뿐 나머지는 정상 동작한다. 그리드 열(_colTimeStamp 등)은
        /// BuildGridColumns()에서 코드로 직접 만들어 이런 손상 자체가 불가능하므로 null 체크가
        /// 필요 없다.</summary>
        public void Initialize(ConnectionManager conn, AppSettings settings)
        {
            _conn = conn;
            _settings = settings;

            SafeSetChecked(_showUsb, settings.MeasurementDisplayChannel != "Uart");
            SafeSetChecked(_showUart, settings.MeasurementDisplayChannel == "Uart");
            SafeSetChecked(_autoScrollCheck, settings.MeasurementAutoScroll);

            _colTimeStamp.Width = settings.MeasurementColTimeStampWidth;
            _colDcIp.Width = settings.MeasurementColDcIpWidth;
            _colMac.Width = settings.MeasurementColMacWidth;
            _colSamples.Width = settings.MeasurementColSamplesWidth;

            if (_conn != null)
            {
                _conn.Usb.LineReceived += OnLineReceived;
                _conn.Uart.LineReceived += OnLineReceived;
            }

            /* MainForm의 상단 4개 스플리터와 동일한 이유로 BeginInvoke를 통해 지연 복원한다:
             * 생성 직후에는 SplitContainer의 Width가 아직 최종값으로 안정되지 않을 수 있다. */
            Load += (s, e) => BeginInvoke(new Action(ApplySavedSplitterDistance));
        }

        /// <summary>control이 null이면 조용히 건너뛴다(<see cref="Initialize"/> 주석 참고).</summary>
        private static void SafeSetChecked(ButtonBase control, bool value)
        {
            if (control is RadioButton radioButton)
            {
                radioButton.Checked = value;
            }
            else if (control is CheckBox checkBox)
            {
                checkBox.Checked = value;
            }
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

        /* 사용자가 그리드 열 폭을 드래그로 조절하면(RawLine 열은 Fill이라 남는 폭을 자동으로
         * 흡수할 뿐 사용자가 직접 조절하는 대상이 아니므로 저장하지 않는다) 즉시 저장하고,
         * 다음 실행 시 Initialize에서 그대로 복원한다. */
        private void Grid_ColumnWidthChanged(object sender, DataGridViewColumnEventArgs e)
        {
            if (_settings == null)
            {
                return;
            }

            if (e.Column == _colTimeStamp)
            {
                _settings.MeasurementColTimeStampWidth = e.Column.Width;
            }
            else if (e.Column == _colDcIp)
            {
                _settings.MeasurementColDcIpWidth = e.Column.Width;
            }
            else if (e.Column == _colMac)
            {
                _settings.MeasurementColMacWidth = e.Column.Width;
            }
            else if (e.Column == _colSamples)
            {
                _settings.MeasurementColSamplesWidth = e.Column.Width;
            }
            else
            {
                return;
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

        private void AutoScrollCheck_CheckedChanged(object sender, EventArgs e)
        {
            if (_settings != null)
            {
                _settings.MeasurementAutoScroll = _autoScrollCheck.Checked;
            }
        }

        private bool IsChannelSelected(LinkChannel channel)
        {
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
                 * 값 포함)는 우측 일반 로그에 원본 그대로 표시한다(채널([USB]/[UART]) 표시는
                 * 붙이지 않는다). */
                _eventLogBox.AppendText(DateTime.Now.ToString("HH:mm:ss.fff") + "  " + payload + Environment.NewLine);

                /* "MAC_<mac address>" 형식이면 그 값만 별도로 MAC Address 표시 영역에도 갱신한다
                 * (위 원본 로그 표시는 그대로 유지한 채 추가로 표시하는 것). */
                if (Stm32Protocol.TryParseMacAddress(payload, out string macAddress))
                {
                    _macAddressValueLabel.Text = macAddress;
                }
            }
        }

        private void ClearButton_Click(object sender, EventArgs e)
        {
            _records.Clear();
            _countLabel.Text = "0건";
            _eventLogBox.Clear();
        }

        /// <summary>CSV로 저장할 열을 화면 그리드와 완전히 동일하게 만들기 위해, 하드코딩된
        /// 열 목록 대신 항상 <c>_grid.Columns</c>에서 그대로 뽑아 쓴다 - 그래서 그리드에서 열을
        /// 뺐다 넣었다 하거나(현재는 TimeStamp/DC IP/MAC/RawLine 4개) 사용자가 열 머리글을
        /// 드래그해 순서를 바꾸면(<c>MeasurementPanel.Designer.cs</c>의
        /// <c>_grid.AllowUserToOrderColumns</c> 참고) CSV도 그 순서/구성 그대로 저장된다. 각
        /// 셀 값도 <c>DataGridViewCell.FormattedValue</c>로 읽어서, TimeStamp 열의
        /// "HH:mm:ss:fff" 형식 등 화면에 보이는 그대로가 CSV에도 쓰인다.</summary>
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
                    DataGridViewColumn[] columns = GetColumnsInDisplayOrder();

                    using (var writer = new StreamWriter(dialog.FileName, false, Encoding.UTF8))
                    {
                        var header = new string[columns.Length];
                        for (int i = 0; i < columns.Length; i++)
                        {
                            header[i] = columns[i].HeaderText;
                        }
                        writer.WriteLine(BuildCsvLine(header));

                        foreach (DataGridViewRow row in _grid.Rows)
                        {
                            var fields = new string[columns.Length];
                            for (int i = 0; i < columns.Length; i++)
                            {
                                object value = row.Cells[columns[i].Index].FormattedValue;
                                fields[i] = value != null ? value.ToString() : string.Empty;
                            }
                            writer.WriteLine(BuildCsvLine(fields));
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

        /// <summary>_grid.Columns를 사용자가 드래그로 바꿔놓았을 수 있는 현재 표시 순서
        /// (DisplayIndex) 그대로 정렬해 반환한다.</summary>
        private DataGridViewColumn[] GetColumnsInDisplayOrder()
        {
            var columns = new DataGridViewColumn[_grid.Columns.Count];
            _grid.Columns.CopyTo(columns, 0);
            Array.Sort(columns, (a, b) => a.DisplayIndex.CompareTo(b.DisplayIndex));
            return columns;
        }

        /// <summary>필드 배열을 CSV 한 줄로 합친다(RFC4180과 유사하게, 콤마/따옴표/줄바꿈이
        /// 포함된 필드만 큰따옴표로 감싸고 내부 따옴표는 두 번 반복). RawLine 열처럼 원본 값
        /// 자체에 콤마가 여러 개 들어있는 필드도 안전하게 한 필드로 유지하기 위함이다.</summary>
        private static string BuildCsvLine(string[] fields)
        {
            var escaped = new string[fields.Length];
            for (int i = 0; i < fields.Length; i++)
            {
                escaped[i] = EscapeCsvField(fields[i]);
            }
            return string.Join(",", escaped);
        }

        private static string EscapeCsvField(string field)
        {
            if (field.IndexOfAny(new[] { ',', '"', '\n', '\r' }) >= 0)
            {
                return "\"" + field.Replace("\"", "\"\"") + "\"";
            }
            return field;
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
