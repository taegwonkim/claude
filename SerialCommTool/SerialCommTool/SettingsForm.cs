using System.IO.Ports;

namespace SerialCommTool
{
    public partial class SettingsForm : Form
    {
        private static readonly int[] CommonBaudRates =
        {
            1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200, 230400
        };

        public SerialSettings Settings { get; private set; }

        public SettingsForm(SerialSettings currentSettings)
        {
            InitializeComponent();
            Settings = currentSettings.Clone();

            cmbBaudRate.Items.AddRange(CommonBaudRates.Select(b => (object)b).ToArray());
            cmbDataBits.Items.AddRange(new object[] { 5, 6, 7, 8 });
            cmbParity.Items.AddRange(Enum.GetNames(typeof(Parity)));
            cmbStopBits.Items.AddRange(new object[] { StopBits.One, StopBits.OnePointFive, StopBits.Two });
            cmbHandshake.Items.AddRange(Enum.GetNames(typeof(Handshake)));

            LoadPortNames();
            ApplySettingsToControls();
        }

        private void LoadPortNames()
        {
            string? previouslySelected = cmbPort.SelectedItem as string;
            cmbPort.Items.Clear();
            cmbPort.Items.AddRange(SerialPort.GetPortNames().OrderBy(p => p).ToArray());

            if (previouslySelected != null && cmbPort.Items.Contains(previouslySelected))
            {
                cmbPort.SelectedItem = previouslySelected;
            }
            else if (cmbPort.Items.Contains(Settings.PortName))
            {
                cmbPort.SelectedItem = Settings.PortName;
            }
            else if (cmbPort.Items.Count > 0)
            {
                cmbPort.SelectedIndex = 0;
            }
        }

        private void ApplySettingsToControls()
        {
            cmbBaudRate.Text = Settings.BaudRate.ToString();
            cmbDataBits.SelectedItem = Settings.DataBits;
            cmbParity.SelectedItem = Settings.Parity.ToString();
            cmbStopBits.SelectedItem = Settings.StopBits;
            cmbHandshake.SelectedItem = Settings.Handshake.ToString();
        }

        private void btnRefreshPorts_Click(object? sender, EventArgs e)
        {
            LoadPortNames();
        }

        private void btnOK_Click(object? sender, EventArgs e)
        {
            if (cmbPort.SelectedItem is not string portName || string.IsNullOrWhiteSpace(portName))
            {
                MessageBox.Show(this, "포트를 선택하세요.", "입력 오류", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                DialogResult = DialogResult.None;
                return;
            }

            if (!int.TryParse(cmbBaudRate.Text, out int baudRate) || baudRate <= 0)
            {
                MessageBox.Show(this, "올바른 속도(Baud Rate) 값을 입력하세요.", "입력 오류", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                DialogResult = DialogResult.None;
                return;
            }

            Settings = new SerialSettings
            {
                PortName = portName,
                BaudRate = baudRate,
                DataBits = (int)cmbDataBits.SelectedItem!,
                Parity = Enum.Parse<Parity>((string)cmbParity.SelectedItem!),
                StopBits = (StopBits)cmbStopBits.SelectedItem!,
                Handshake = Enum.Parse<Handshake>((string)cmbHandshake.SelectedItem!)
            };
        }
    }
}
