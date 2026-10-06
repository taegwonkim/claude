using System;
using System.Collections.Generic;
using System.Drawing;
using System.IO;
using System.Text;
using System.Windows.Forms;
using Stm32WifiConfigTool.Models;
using Stm32WifiConfigTool.Services;

namespace Stm32WifiConfigTool.Panels
{
    /// <summary>
    /// ESP32 상태("STATUS,&lt;번호&gt;" 또는 실측 형식 "STATUS:&lt;번호&gt;", STX 유무 무관 -
    /// <see cref="Stm32Protocol.TryParseStatusText"/> 참고) 표시 패널. MCU는 측정값 전송 사이사이에
    /// 이 프레임을 주기적으로 브로드캐스트한다(docs/프로토콜_명세.md §1). 측정값 프레임과는
    /// 별도로 구분해서 여기 표시한다. UART로 수신한 값을 표시한다.
    /// STATUS 값은 위 "현재 ESP32 상태"(큰 글씨)만 갱신하며, 아래 "수신 이력"에는 더 이상
    /// 기록하지 않는다 - 대신 "[RESET]"로 시작하는 소프트웨어 리셋 로그 줄(예: "[RESET]
    /// Software Reset Count: 0", <see cref="Stm32Protocol.IsResetLogText"/> 참고)이 오면
    /// 그 원본 텍스트를 그대로 수신 이력에 기록한다. 각 줄 맨 앞의 시각은
    /// "yyyy-MM-dd HH:mm:ss"(24시간제) 형식이다. "CSV로 저장"으로 지금까지 쌓인 수신 이력
    /// (TimeStamp/Message 두 열)을 CSV 파일로 내보낼 수 있다(<see cref="ExportButton_Click"/> 참고).
    /// UI 레이아웃은 <c>EspStatusPanel.Designer.cs</c>에 있으며 Visual Studio
    /// 디자이너로 편집 가능하다. 매개변수 없는 생성자는 디자이너 전용이며, 실제 사용 시에는
    /// 생성 직후 <see cref="Initialize"/>를 호출해 런타임 의존성(ConnectionManager, AppSettings)을
    /// 연결해야 한다. MainForm에 다른 패널들과 함께 한 창에 도킹되어 표시된다.
    /// </summary>
    public partial class EspStatusPanel : UserControl
    {
        private const int MaxLogLines = 2000;

        private ConnectionManager _conn;
        private AppSettings _settings;
        private int _logLineCount;

        /// <summary>_logBox에 쌓인 "[RESET]" 수신 이력과 1:1로 대응하는 구조화된 기록 - CSV로
        /// 저장할 때 이 목록을 그대로 쓴다(<see cref="ExportButton_Click"/> 참고). _logBox가
        /// MaxLogLines를 넘겨 오래된 줄을 잘라낼 때 이 목록의 맨 앞도 함께 제거해 항상 화면에
        /// 보이는 줄과 동일한 내용을 유지한다.</summary>
        private readonly List<(string TimeStamp, string Message)> _logRecords = new List<(string, string)>();

        public EspStatusPanel()
        {
            InitializeComponent();
        }

        /// <summary>디자이너가 만든 컨트롤에 실제 동작을 연결한다. MainForm이 생성 직후 1회 호출.</summary>
        public void Initialize(ConnectionManager conn, AppSettings settings)
        {
            _conn = conn;
            _settings = settings;

            _conn.Uart.LineReceived += OnLineReceived;
        }

        private static Color ColorForStatus(int statusNumber)
        {
            switch (statusNumber)
            {
                case 0: return Color.Firebrick;
                case 1: return Color.DarkOrange;
                case 2: return Color.SeaGreen;
                default: return Color.Gray;
            }
        }

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
            /* STX 유무와 관계없이 처리한다(실측 결과 MCU가 모든 프레임에 STX를 붙이지는 않음). */
            string payload = Stm32Protocol.DisplayText(line);

            /* STATUS는 위 "현재 ESP32 상태"만 갱신하고, 수신 이력에는 더 이상 기록하지 않는다
             * (콤마 "STATUS,<번호>"와 콜론 "STATUS:<번호>" 형식을 모두 인식한다). */
            if (Stm32Protocol.TryParseStatusText(payload, out int statusNumber))
            {
                string text = Stm32Protocol.DescribeStatus(statusNumber);
                _currentStatusLabel.Text = text + " (" + statusNumber + ")";
                _currentStatusLabel.ForeColor = ColorForStatus(statusNumber);
                _lastUpdateLabel.Text = "마지막 수신: " + DateTime.Now.ToString("HH:mm:ss.fff");
                return;
            }

            /* 수신 이력에는 대신 소프트웨어 리셋 로그("[RESET] ...")를 기록한다. */
            if (Stm32Protocol.IsResetLogText(payload))
            {
                AppendLog(payload);
            }
        }

        private void AppendLog(string message)
        {
            string timeStamp = DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss");
            _logBox.AppendText(timeStamp + "  " + message + Environment.NewLine);
            _logRecords.Add((timeStamp, message));
            _logLineCount++;

            if (_logLineCount > MaxLogLines)
            {
                /* 오래된 줄부터 잘라내 메모리를 보호한다 - _logRecords도 화면과 같은 줄만
                 * 남도록 맨 앞을 함께 제거한다. */
                int cut = _logBox.Text.IndexOf('\n');
                if (cut >= 0)
                {
                    _logBox.Text = _logBox.Text.Substring(cut + 1);
                    _logLineCount--;
                }
                if (_logRecords.Count > 0)
                {
                    _logRecords.RemoveAt(0);
                }
            }
        }

        private void ClearButton_Click(object sender, EventArgs e)
        {
            _logBox.Clear();
            _logRecords.Clear();
            _logLineCount = 0;
            _currentStatusLabel.Text = "-";
            _currentStatusLabel.ForeColor = Color.Gray;
            _lastUpdateLabel.Text = "수신 대기 중...";
        }

        /// <summary>지금까지 쌓인 "[RESET]" 수신 이력(<see cref="_logRecords"/>)을 CSV 파일로
        /// 내보낸다. 열은 TimeStamp/Message 두 개이며, 콤마/따옴표/줄바꿈이 포함된 값(예: Message)은
        /// MeasurementPanel의 CSV 내보내기와 같은 방식으로(RFC4180과 유사하게) 큰따옴표로
        /// 감싼다.</summary>
        private void ExportButton_Click(object sender, EventArgs e)
        {
            if (_logRecords.Count == 0)
            {
                MessageBox.Show(this, "저장할 수신 이력이 없습니다.", "CSV로 저장", MessageBoxButtons.OK, MessageBoxIcon.Information);
                return;
            }

            using (var dialog = new SaveFileDialog { Filter = "CSV 파일|*.csv", FileName = "esp_status_log_" + DateTime.Now.ToString("yyyyMMdd_HHmmss") + ".csv" })
            {
                if (dialog.ShowDialog(this) != DialogResult.OK)
                {
                    return;
                }

                try
                {
                    using (var writer = new StreamWriter(dialog.FileName, false, Encoding.UTF8))
                    {
                        writer.WriteLine(BuildCsvLine(new[] { "TimeStamp", "Message" }));
                        foreach ((string timeStamp, string message) in _logRecords)
                        {
                            writer.WriteLine(BuildCsvLine(new[] { timeStamp, message }));
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
                    _conn.Uart.LineReceived -= OnLineReceived;
                }
                components?.Dispose();
            }
            base.Dispose(disposing);
        }
    }
}
