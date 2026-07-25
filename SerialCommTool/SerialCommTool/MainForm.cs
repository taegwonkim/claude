using System.IO.Ports;
using System.Text;

namespace SerialCommTool
{
    public partial class MainForm : Form
    {
        private SerialSettings _settings = new();

        public MainForm()
        {
            InitializeComponent();
            UpdateSettingsLabel();
            UpdateConnectionUi(false);
        }

        private void UpdateSettingsLabel()
        {
            lblSettingsInfo.Text = $"설정: {_settings}";
        }

        private void UpdateConnectionUi(bool connected)
        {
            btnConnect.Text = connected ? "연결 해제" : "연결";
            btnSettings.Enabled = !connected;
            miSettings.Enabled = !connected;
            toolStripStatusLabelConnection.Text = connected ? $"연결됨: {_settings}" : "연결 안됨";
        }

        private void btnSettings_Click(object? sender, EventArgs e)
        {
            using var form = new SettingsForm(_settings);
            if (form.ShowDialog(this) == DialogResult.OK)
            {
                _settings = form.Settings;
                UpdateSettingsLabel();
            }
        }

        private void btnConnect_Click(object? sender, EventArgs e)
        {
            if (serialPort1.IsOpen)
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
            serialPort1.PortName = _settings.PortName;
            serialPort1.BaudRate = _settings.BaudRate;
            serialPort1.DataBits = _settings.DataBits;
            serialPort1.Parity = _settings.Parity;
            serialPort1.StopBits = _settings.StopBits;
            serialPort1.Handshake = _settings.Handshake;

            try
            {
                serialPort1.Open();
                UpdateConnectionUi(true);
            }
            catch (Exception ex)
            {
                MessageBox.Show(this, $"포트를 여는 중 오류가 발생했습니다.\n{ex.Message}", "연결 오류",
                    MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        private void Disconnect()
        {
            try
            {
                serialPort1.Close();
            }
            finally
            {
                UpdateConnectionUi(false);
            }
        }

        private void serialPort1_DataReceived(object sender, SerialDataReceivedEventArgs e)
        {
            byte[] buffer;
            try
            {
                int bytesToRead = serialPort1.BytesToRead;
                if (bytesToRead == 0) return;
                buffer = new byte[bytesToRead];
                serialPort1.Read(buffer, 0, bytesToRead);
            }
            catch (Exception)
            {
                return;
            }

            AppendReceivedData(buffer);
        }

        private void AppendReceivedData(byte[] data)
        {
            if (IsDisposed || !IsHandleCreated) return;

            void Append()
            {
                string text = chkHexView.Checked ? BytesToHex(data) + " " : Encoding.ASCII.GetString(data);
                txtReceive.AppendText(text);
            }

            try
            {
                if (InvokeRequired)
                {
                    BeginInvoke((Action)Append);
                }
                else
                {
                    Append();
                }
            }
            catch (ObjectDisposedException)
            {
            }
        }

        private void btnSend_Click(object? sender, EventArgs e)
        {
            if (!serialPort1.IsOpen)
            {
                MessageBox.Show(this, "먼저 포트에 연결하세요.", "알림", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return;
            }

            string input = txtSend.Text;
            if (input.Length == 0) return;

            try
            {
                byte[] data;
                if (chkHexSend.Checked)
                {
                    data = HexStringToBytes(input);
                    if (chkAppendNewLine.Checked)
                    {
                        data = data.Concat(new byte[] { 0x0D, 0x0A }).ToArray();
                    }
                }
                else
                {
                    string toSend = chkAppendNewLine.Checked ? input + "\r\n" : input;
                    data = Encoding.ASCII.GetBytes(toSend);
                }

                serialPort1.Write(data, 0, data.Length);
                txtReceive.AppendText($">> {(chkHexSend.Checked ? BytesToHex(data) : input)}{Environment.NewLine}");
            }
            catch (FormatException)
            {
                MessageBox.Show(this, "16진수 형식이 올바르지 않습니다. 예: 01 0A FF", "입력 오류",
                    MessageBoxButtons.OK, MessageBoxIcon.Warning);
            }
            catch (Exception ex)
            {
                MessageBox.Show(this, $"데이터 송신 중 오류가 발생했습니다.\n{ex.Message}", "송신 오류",
                    MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        private void txtSend_KeyDown(object? sender, KeyEventArgs e)
        {
            if (e.KeyCode == Keys.Enter && !e.Shift)
            {
                e.SuppressKeyPress = true;
                btnSend_Click(sender, EventArgs.Empty);
            }
        }

        private void btnClearReceive_Click(object? sender, EventArgs e)
        {
            txtReceive.Clear();
        }

        private void miExit_Click(object? sender, EventArgs e)
        {
            Close();
        }

        private void MainForm_FormClosing(object? sender, FormClosingEventArgs e)
        {
            if (serialPort1.IsOpen)
            {
                serialPort1.Close();
            }
        }

        private static string BytesToHex(byte[] data)
        {
            return string.Join(' ', data.Select(b => b.ToString("X2")));
        }

        private static byte[] HexStringToBytes(string hex)
        {
            var tokens = hex.Split(new[] { ' ', '\t', '\r', '\n', '-', ',' }, StringSplitOptions.RemoveEmptyEntries);
            var result = new byte[tokens.Length];
            for (int i = 0; i < tokens.Length; i++)
            {
                result[i] = Convert.ToByte(tokens[i], 16);
            }
            return result;
        }
    }
}
