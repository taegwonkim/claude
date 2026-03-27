using System;
using System.Drawing;
using System.IO.Ports;
using System.Text;
using System.Windows.Forms;

namespace UartCommunicator
{
    /// <summary>
    /// UART 시리얼 통신 메인 폼
    /// </summary>
    public partial class MainForm : Form
    {
        // ──────────────────────────────────────────────────────────────────────
        // 필드
        // ──────────────────────────────────────────────────────────────────────

        private SerialPort?  _port;
        private ViewMode     _viewMode   = ViewMode.Ascii;
        private readonly object _portLock = new();

        private enum ViewMode { Ascii, Hex }

        // ──────────────────────────────────────────────────────────────────────
        // 생성자
        // ──────────────────────────────────────────────────────────────────────

        public MainForm()
        {
            InitializeComponent();
            InitPortSettings();
            RefreshPortList();
            SetConnectionState(false);
        }

        // ──────────────────────────────────────────────────────────────────────
        // 초기화 헬퍼
        // ──────────────────────────────────────────────────────────────────────

        private void InitPortSettings()
        {
            // Baud Rate
            int[] baudRates = { 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600 };
            foreach (int b in baudRates) cmbBaudRate.Items.Add(b);
            cmbBaudRate.SelectedItem = 115200;

            // Data Bits
            cmbDataBits.Items.AddRange(new object[] { 5, 6, 7, 8 });
            cmbDataBits.SelectedIndex = 3; // 8

            // Parity
            cmbParity.Items.AddRange(new object[] { "None", "Odd", "Even", "Mark", "Space" });
            cmbParity.SelectedIndex = 0; // None

            // Stop Bits
            cmbStopBits.Items.AddRange(new object[] { "One", "Two", "OnePointFive" });
            cmbStopBits.SelectedIndex = 0; // One

            // Flow Control
            cmbFlowControl.Items.AddRange(new object[] { "None", "RequestToSend", "XOnXOff", "RequestToSendXOnXOff" });
            cmbFlowControl.SelectedIndex = 0;
        }

        private void RefreshPortList()
        {
            string? previousPort = cmbPort.SelectedItem?.ToString();
            cmbPort.Items.Clear();

            string[] ports = SerialPort.GetPortNames();
            Array.Sort(ports);   // COM1, COM2 … 순서 정렬

            if (ports.Length == 0)
            {
                cmbPort.Items.Add("(포트 없음)");
                cmbPort.SelectedIndex = 0;
            }
            else
            {
                cmbPort.Items.AddRange(ports);
                int prev = cmbPort.Items.IndexOf(previousPort ?? "");
                cmbPort.SelectedIndex = prev >= 0 ? prev : 0;
            }
        }

        private void SetConnectionState(bool connected)
        {
            // 포트 설정 컨트롤 – 연결 시 잠금
            cmbPort.Enabled         = !connected;
            cmbBaudRate.Enabled     = !connected;
            cmbDataBits.Enabled     = !connected;
            cmbParity.Enabled       = !connected;
            cmbStopBits.Enabled     = !connected;
            cmbFlowControl.Enabled  = !connected;
            btnRefresh.Enabled      = !connected;

            // 송신 컨트롤 – 연결 시 활성화
            btnSend.Enabled  = connected;
            txtSend.Enabled  = connected;
            btnClearTx.Enabled = connected;

            // 연결 버튼 스타일
            if (connected)
            {
                btnConnect.Text      = "연결 해제 (F5)";
                btnConnect.BackColor = Color.FromArgb(192, 57, 43);
            }
            else
            {
                btnConnect.Text      = "연결 (F5)";
                btnConnect.BackColor = Color.FromArgb(39, 174, 96);
            }

            // 상태 표시줄
            if (connected && _port != null)
            {
                toolStripStatusConn.Text      = $"  연결됨: {_port.PortName}  |  {_port.BaudRate} bps  |  {_port.DataBits}-{_port.Parity}-{_port.StopBits}";
                toolStripStatusConn.ForeColor = Color.LimeGreen;
            }
            else
            {
                toolStripStatusConn.Text      = "  연결 안됨";
                toolStripStatusConn.ForeColor = Color.Tomato;
            }
        }

        // ──────────────────────────────────────────────────────────────────────
        // 포트 새로 고침
        // ──────────────────────────────────────────────────────────────────────

        private void btnRefresh_Click(object sender, EventArgs e) => RefreshPortList();

        // ──────────────────────────────────────────────────────────────────────
        // 연결 / 해제 (F5 단축키 포함)
        // ──────────────────────────────────────────────────────────────────────

        protected override bool ProcessCmdKey(ref Message msg, Keys keyData)
        {
            if (keyData == Keys.F5) { ToggleConnection(); return true; }
            return base.ProcessCmdKey(ref msg, keyData);
        }

        private void btnConnect_Click(object sender, EventArgs e) => ToggleConnection();

        private void ToggleConnection()
        {
            if (_port != null && _port.IsOpen) Disconnect();
            else                               Connect();
        }

        private void Connect()
        {
            if (cmbPort.SelectedItem is null || cmbPort.SelectedItem.ToString() == "(포트 없음)")
            {
                MessageBox.Show("유효한 COM 포트를 선택하세요.", "포트 오류",
                    MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return;
            }

            try
            {
                _port = new SerialPort
                {
                    PortName     = cmbPort.SelectedItem.ToString()!,
                    BaudRate     = (int)cmbBaudRate.SelectedItem!,
                    DataBits     = (int)cmbDataBits.SelectedItem!,
                    Parity       = Enum.Parse<Parity>(cmbParity.SelectedItem!.ToString()!),
                    StopBits     = Enum.Parse<StopBits>(cmbStopBits.SelectedItem!.ToString()!),
                    Handshake    = Enum.Parse<Handshake>(cmbFlowControl.SelectedItem!.ToString()!),
                    ReadTimeout  = 500,
                    WriteTimeout = 500,
                    Encoding     = Encoding.Latin1   // raw byte pass-through
                };

                _port.DataReceived  += Port_DataReceived;
                _port.ErrorReceived += Port_ErrorReceived;
                _port.Open();

                AppendSystem($"[연결] {_port.PortName} @ {_port.BaudRate} bps  " +
                             $"({_port.DataBits} 데이터비트, 패리티:{_port.Parity}, 정지:{_port.StopBits})");
                SetConnectionState(true);
            }
            catch (Exception ex)
            {
                MessageBox.Show($"포트를 열 수 없습니다:\n{ex.Message}", "연결 오류",
                    MessageBoxButtons.OK, MessageBoxIcon.Error);
                _port?.Dispose();
                _port = null;
            }
        }

        private void Disconnect()
        {
            lock (_portLock)
            {
                if (_port != null)
                {
                    _port.DataReceived  -= Port_DataReceived;
                    _port.ErrorReceived -= Port_ErrorReceived;
                    if (_port.IsOpen) _port.Close();
                    _port.Dispose();
                    _port = null;
                }
            }
            AppendSystem("[연결 해제]");
            SetConnectionState(false);
        }

        // ──────────────────────────────────────────────────────────────────────
        // 수신 이벤트
        // ──────────────────────────────────────────────────────────────────────

        private void Port_DataReceived(object sender, SerialDataReceivedEventArgs e)
        {
            SerialPort? p;
            lock (_portLock) { p = _port; }
            if (p == null || !p.IsOpen) return;

            try
            {
                int count = p.BytesToRead;
                byte[] buf = new byte[count];
                p.Read(buf, 0, count);
                Invoke(() => DisplayReceived(buf));
            }
            catch { /* 읽는 도중 포트가 닫힌 경우 무시 */ }
        }

        private void Port_ErrorReceived(object sender, SerialErrorReceivedEventArgs e)
        {
            Invoke(() => AppendSystem($"[시리얼 오류] {e.EventType}"));
        }

        private void DisplayReceived(byte[] data)
        {
            string text;
            if (_viewMode == ViewMode.Hex)
            {
                var sb = new StringBuilder(data.Length * 3);
                foreach (byte b in data)
                    sb.Append($"{b:X2} ");
                text = sb.ToString();
            }
            else
            {
                text = Encoding.Latin1.GetString(data);
            }

            AppendRx(text);
            UpdateByteCount(data.Length);
        }

        // ──────────────────────────────────────────────────────────────────────
        // 송신
        // ──────────────────────────────────────────────────────────────────────

        private void btnSend_Click(object sender, EventArgs e) => SendData();

        private void txtSend_KeyDown(object sender, KeyEventArgs e)
        {
            // Shift+Enter = 줄 바꿈, Enter 단독 = 송신
            if (e.KeyCode == Keys.Enter && !e.Shift)
            {
                e.SuppressKeyPress = true;
                SendData();
            }
        }

        private void SendData()
        {
            if (_port == null || !_port.IsOpen) return;

            string raw = txtSend.Text;
            if (string.IsNullOrEmpty(raw)) return;

            try
            {
                byte[] data;

                if (menuSendHex.Checked)
                {
                    data = ParseHexString(raw);
                    if (data is null) return;
                    AppendTx($"[TX-HEX] {raw.Trim().ToUpperInvariant()}");
                }
                else
                {
                    string toSend = chkCRLF.Checked ? raw + "\r\n" : raw;
                    data = Encoding.Latin1.GetBytes(toSend);
                    AppendTx($"[TX] {raw}");
                }

                _port.Write(data, 0, data.Length);
                if (chkClearSend.Checked) txtSend.Clear();
            }
            catch (Exception ex)
            {
                AppendSystem($"[송신 오류] {ex.Message}");
            }
        }

        private static byte[]? ParseHexString(string hex)
        {
            hex = hex.Replace(" ", "").Replace("\t", "").Replace("\r", "").Replace("\n", "");
            if (hex.Length % 2 != 0)
            {
                MessageBox.Show("HEX 문자열의 길이가 짝수가 아닙니다.\n예: 01 A2 FF 3C",
                    "HEX 형식 오류", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return null;
            }
            try
            {
                byte[] result = new byte[hex.Length / 2];
                for (int i = 0; i < result.Length; i++)
                    result[i] = Convert.ToByte(hex.Substring(i * 2, 2), 16);
                return result;
            }
            catch
            {
                MessageBox.Show("유효하지 않은 HEX 문자열입니다.", "HEX 형식 오류",
                    MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return null;
            }
        }

        // ──────────────────────────────────────────────────────────────────────
        // RichTextBox 출력 헬퍼
        // ──────────────────────────────────────────────────────────────────────

        private static readonly Color ColorRx     = Color.FromArgb(50, 215, 100);   // 초록 – 수신
        private static readonly Color ColorTx     = Color.FromArgb(100, 180, 255);  // 파랑 – 송신
        private static readonly Color ColorSystem = Color.FromArgb(200, 180, 60);   // 노랑 – 시스템

        private void AppendRx(string text)     => AppendColored(text,   ColorRx,     false);
        private void AppendTx(string text)     => AppendColored(text,   ColorTx,     true);
        private void AppendSystem(string text) => AppendColored(text,   ColorSystem, true);

        private void AppendColored(string text, Color color, bool newLine)
        {
            rtbReceive.SuspendLayout();
            if (newLine && rtbReceive.TextLength > 0)
                rtbReceive.AppendText(Environment.NewLine);

            rtbReceive.SelectionStart  = rtbReceive.TextLength;
            rtbReceive.SelectionLength = 0;
            rtbReceive.SelectionColor  = color;
            rtbReceive.AppendText(text);
            rtbReceive.SelectionColor  = rtbReceive.ForeColor;

            if (chkAutoScroll.Checked)
            {
                rtbReceive.SelectionStart = rtbReceive.TextLength;
                rtbReceive.ScrollToCaret();
            }
            rtbReceive.ResumeLayout();
        }

        private int _totalBytesReceived = 0;

        private void UpdateByteCount(int n)
        {
            _totalBytesReceived += n;
            toolStripStatusBytes.Text = $"  수신: {_totalBytesReceived} bytes";
        }

        // ──────────────────────────────────────────────────────────────────────
        // 메뉴 – 보기 (View)
        // ──────────────────────────────────────────────────────────────────────

        private void menuViewAscii_Click(object sender, EventArgs e)
        {
            _viewMode              = ViewMode.Ascii;
            menuViewAscii.Checked  = true;
            menuViewHex.Checked    = false;
            toolStripStatusView.Text = "  보기: ASCII";
        }

        private void menuViewHex_Click(object sender, EventArgs e)
        {
            _viewMode              = ViewMode.Hex;
            menuViewAscii.Checked  = false;
            menuViewHex.Checked    = true;
            toolStripStatusView.Text = "  보기: HEX";
        }

        // ──────────────────────────────────────────────────────────────────────
        // 메뉴 – 송신 형식
        // ──────────────────────────────────────────────────────────────────────

        private void menuSendAscii_Click(object sender, EventArgs e)
        {
            menuSendAscii.Checked = true;
            menuSendHex.Checked   = false;
            grpSend.Text          = "송신 데이터  (ASCII)";
        }

        private void menuSendHex_Click(object sender, EventArgs e)
        {
            menuSendHex.Checked   = true;
            menuSendAscii.Checked = false;
            grpSend.Text          = "송신 데이터  (HEX — 예: 01 A2 FF)";
        }

        // ──────────────────────────────────────────────────────────────────────
        // 메뉴 – 편집
        // ──────────────────────────────────────────────────────────────────────

        private void menuClearRx_Click(object sender, EventArgs e)
        {
            rtbReceive.Clear();
            _totalBytesReceived = 0;
            toolStripStatusBytes.Text = "  수신: 0 bytes";
        }

        private void btnClearTx_Click(object sender, EventArgs e) => txtSend.Clear();

        private void menuCopyRx_Click(object sender, EventArgs e)
        {
            if (!string.IsNullOrEmpty(rtbReceive.Text))
                Clipboard.SetText(rtbReceive.Text);
        }

        // ──────────────────────────────────────────────────────────────────────
        // 메뉴 – 파일
        // ──────────────────────────────────────────────────────────────────────

        private void menuSaveLog_Click(object sender, EventArgs e)
        {
            using var dlg = new SaveFileDialog
            {
                Title    = "로그 저장",
                Filter   = "텍스트 파일 (*.txt)|*.txt|모든 파일 (*.*)|*.*",
                FileName = $"uart_log_{DateTime.Now:yyyyMMdd_HHmmss}.txt"
            };
            if (dlg.ShowDialog() == DialogResult.OK)
            {
                System.IO.File.WriteAllText(dlg.FileName, rtbReceive.Text, Encoding.UTF8);
                MessageBox.Show("저장 완료!", "저장", MessageBoxButtons.OK, MessageBoxIcon.Information);
            }
        }

        private void menuExit_Click(object sender, EventArgs e)
        {
            Disconnect();
            Application.Exit();
        }

        // ──────────────────────────────────────────────────────────────────────
        // 폼 닫기
        // ──────────────────────────────────────────────────────────────────────

        protected override void OnFormClosing(FormClosingEventArgs e)
        {
            Disconnect();
            base.OnFormClosing(e);
        }
    }
}
