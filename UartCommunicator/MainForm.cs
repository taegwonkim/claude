using System;
using System.Drawing;
using System.IO.Ports;
using System.Text;
using System.Windows.Forms;

namespace UartCommunicator
{
    public partial class MainForm : Form
    {
        private SerialPort? _serialPort;
        private ViewMode _viewMode = ViewMode.Ascii;
        private readonly StringBuilder _receiveBuffer = new();

        private enum ViewMode { Ascii, Hex }

        public MainForm()
        {
            InitializeComponent();
            PopulatePortList();
            PopulateBaudRates();
            UpdateConnectionState(false);
        }

        // ──────────────────────────────────────────────
        // Initialization helpers
        // ──────────────────────────────────────────────

        private void PopulatePortList()
        {
            cmbPort.Items.Clear();
            string[] ports = SerialPort.GetPortNames();
            if (ports.Length == 0)
            {
                cmbPort.Items.Add("(포트 없음)");
                cmbPort.SelectedIndex = 0;
            }
            else
            {
                cmbPort.Items.AddRange(ports);
                cmbPort.SelectedIndex = 0;
            }
        }

        private void PopulateBaudRates()
        {
            int[] rates = { 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600 };
            foreach (int r in rates)
                cmbBaudRate.Items.Add(r);
            cmbBaudRate.SelectedItem = 115200;
        }

        private void UpdateConnectionState(bool connected)
        {
            btnConnect.Text = connected ? "연결 해제" : "연결";
            btnConnect.BackColor = connected ? Color.IndianRed : Color.MediumSeaGreen;
            cmbPort.Enabled = !connected;
            cmbBaudRate.Enabled = !connected;
            cmbDataBits.Enabled = !connected;
            cmbParity.Enabled = !connected;
            cmbStopBits.Enabled = !connected;
            btnRefreshPorts.Enabled = !connected;
            btnSend.Enabled = connected;
            txtSend.Enabled = connected;
            statusLabel.Text = connected
                ? $"연결됨: {_serialPort?.PortName} @ {_serialPort?.BaudRate} bps"
                : "연결 안됨";
            statusLabel.ForeColor = connected ? Color.DarkGreen : Color.DarkRed;
        }

        // ──────────────────────────────────────────────
        // Port refresh
        // ──────────────────────────────────────────────

        private void btnRefreshPorts_Click(object sender, EventArgs e)
        {
            PopulatePortList();
        }

        // ──────────────────────────────────────────────
        // Connect / Disconnect
        // ──────────────────────────────────────────────

        private void btnConnect_Click(object sender, EventArgs e)
        {
            if (_serialPort != null && _serialPort.IsOpen)
            {
                Disconnect();
            }
            else
            {
                Connect();
            }
        }

        private void Connect()
        {
            if (cmbPort.SelectedItem == null || cmbPort.SelectedItem.ToString() == "(포트 없음)")
            {
                MessageBox.Show("유효한 포트를 선택해주세요.", "오류", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return;
            }

            try
            {
                _serialPort = new SerialPort
                {
                    PortName  = cmbPort.SelectedItem.ToString()!,
                    BaudRate  = (int)cmbBaudRate.SelectedItem!,
                    DataBits  = int.Parse(cmbDataBits.SelectedItem!.ToString()!),
                    Parity    = (Parity)Enum.Parse(typeof(Parity), cmbParity.SelectedItem!.ToString()!),
                    StopBits  = (StopBits)Enum.Parse(typeof(StopBits), cmbStopBits.SelectedItem!.ToString()!),
                    ReadTimeout  = 500,
                    WriteTimeout = 500,
                    Encoding  = Encoding.Latin1   // raw byte pass-through
                };

                _serialPort.DataReceived += SerialPort_DataReceived;
                _serialPort.ErrorReceived += SerialPort_ErrorReceived;
                _serialPort.Open();

                AppendLog($"[시스템] {_serialPort.PortName} 연결됨 ({_serialPort.BaudRate} bps)");
                UpdateConnectionState(true);
            }
            catch (Exception ex)
            {
                MessageBox.Show($"포트 열기 실패:\n{ex.Message}", "오류", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        private void Disconnect()
        {
            if (_serialPort != null)
            {
                _serialPort.DataReceived -= SerialPort_DataReceived;
                _serialPort.ErrorReceived -= SerialPort_ErrorReceived;
                if (_serialPort.IsOpen) _serialPort.Close();
                _serialPort.Dispose();
                _serialPort = null;
            }
            AppendLog("[시스템] 연결 해제됨");
            UpdateConnectionState(false);
        }

        // ──────────────────────────────────────────────
        // Serial data received
        // ──────────────────────────────────────────────

        private void SerialPort_DataReceived(object sender, SerialDataReceivedEventArgs e)
        {
            if (_serialPort == null || !_serialPort.IsOpen) return;
            try
            {
                int bytesToRead = _serialPort.BytesToRead;
                byte[] buffer = new byte[bytesToRead];
                _serialPort.Read(buffer, 0, bytesToRead);
                Invoke(new Action(() => ProcessReceivedBytes(buffer)));
            }
            catch { /* port closed mid-read */ }
        }

        private void SerialPort_ErrorReceived(object sender, SerialErrorReceivedEventArgs e)
        {
            Invoke(new Action(() => AppendLog($"[오류] 시리얼 오류: {e.EventType}")));
        }

        private void ProcessReceivedBytes(byte[] data)
        {
            if (_viewMode == ViewMode.Hex)
            {
                var sb = new StringBuilder();
                foreach (byte b in data)
                    sb.Append($"{b:X2} ");
                AppendReceive(sb.ToString().TrimEnd());
            }
            else
            {
                AppendReceive(Encoding.Latin1.GetString(data));
            }
        }

        // ──────────────────────────────────────────────
        // Send
        // ──────────────────────────────────────────────

        private void btnSend_Click(object sender, EventArgs e) => SendData();

        private void txtSend_KeyDown(object sender, KeyEventArgs e)
        {
            if (e.KeyCode == Keys.Enter && !e.Shift)
            {
                e.SuppressKeyPress = true;
                SendData();
            }
        }

        private void SendData()
        {
            if (_serialPort == null || !_serialPort.IsOpen) return;

            string text = txtSend.Text;
            if (string.IsNullOrEmpty(text)) return;

            try
            {
                byte[] data;

                if (chkHexSend.Checked)
                {
                    // Parse hex string like "01 02 FF A0"
                    data = ParseHexString(text);
                    if (data == null) return;
                    AppendLog($"[TX-HEX] {text.Trim().ToUpper()}");
                }
                else
                {
                    string toSend = chkNewline.Checked ? text + "\r\n" : text;
                    data = Encoding.Latin1.GetBytes(toSend);
                    AppendLog($"[TX] {text}");
                }

                _serialPort.Write(data, 0, data.Length);

                if (chkClearOnSend.Checked)
                    txtSend.Clear();
            }
            catch (Exception ex)
            {
                AppendLog($"[오류] 송신 실패: {ex.Message}");
            }
        }

        private byte[]? ParseHexString(string hex)
        {
            hex = hex.Replace(" ", "").Replace("\t", "").Replace("\r", "").Replace("\n", "");
            if (hex.Length % 2 != 0)
            {
                MessageBox.Show("HEX 문자열의 길이가 짝수가 아닙니다.", "HEX 오류", MessageBoxButtons.OK, MessageBoxIcon.Warning);
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
                MessageBox.Show("유효하지 않은 HEX 문자열입니다.", "HEX 오류", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return null;
            }
        }

        // ──────────────────────────────────────────────
        // Receive display helpers
        // ──────────────────────────────────────────────

        private void AppendReceive(string text)
        {
            rtbReceive.SelectionColor = Color.LimeGreen;
            rtbReceive.AppendText(text);
            if (chkAutoScroll.Checked)
                rtbReceive.ScrollToCaret();
        }

        private void AppendLog(string text)
        {
            rtbReceive.SelectionColor = Color.DodgerBlue;
            rtbReceive.AppendText(Environment.NewLine + text + Environment.NewLine);
            if (chkAutoScroll.Checked)
                rtbReceive.ScrollToCaret();
        }

        // ──────────────────────────────────────────────
        // Menu – View
        // ──────────────────────────────────────────────

        private void menuViewAscii_Click(object sender, EventArgs e)
        {
            _viewMode = ViewMode.Ascii;
            menuViewAscii.Checked = true;
            menuViewHex.Checked   = false;
            viewModeLabel.Text    = "보기: ASCII";
        }

        private void menuViewHex_Click(object sender, EventArgs e)
        {
            _viewMode = ViewMode.Hex;
            menuViewAscii.Checked = false;
            menuViewHex.Checked   = true;
            viewModeLabel.Text    = "보기: HEX";
        }

        // ──────────────────────────────────────────────
        // Menu – Edit / Clear
        // ──────────────────────────────────────────────

        private void menuClearReceive_Click(object sender, EventArgs e) => rtbReceive.Clear();
        private void menuClearSend_Click(object sender, EventArgs e)    => txtSend.Clear();

        // ──────────────────────────────────────────────
        // Menu – File
        // ──────────────────────────────────────────────

        private void menuSaveLog_Click(object sender, EventArgs e)
        {
            using var dlg = new SaveFileDialog
            {
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

        // ──────────────────────────────────────────────
        // Form closing
        // ──────────────────────────────────────────────

        protected override void OnFormClosing(FormClosingEventArgs e)
        {
            Disconnect();
            base.OnFormClosing(e);
        }
    }
}
