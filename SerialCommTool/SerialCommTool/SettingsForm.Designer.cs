namespace SerialCommTool
{
    partial class SettingsForm
    {
        private System.ComponentModel.IContainer components = null;

        private TableLayoutPanel tableLayoutPanel1;
        private Label lblPort;
        private ComboBox cmbPort;
        private Button btnRefreshPorts;
        private Label lblBaudRate;
        private ComboBox cmbBaudRate;
        private Label lblDataBits;
        private ComboBox cmbDataBits;
        private Label lblParity;
        private ComboBox cmbParity;
        private Label lblStopBits;
        private ComboBox cmbStopBits;
        private Label lblHandshake;
        private ComboBox cmbHandshake;
        private FlowLayoutPanel flowLayoutPanelButtons;
        private Button btnOK;
        private Button btnCancel;

        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        private void InitializeComponent()
        {
            tableLayoutPanel1 = new TableLayoutPanel();
            lblPort = new Label();
            cmbPort = new ComboBox();
            btnRefreshPorts = new Button();
            lblBaudRate = new Label();
            cmbBaudRate = new ComboBox();
            lblDataBits = new Label();
            cmbDataBits = new ComboBox();
            lblParity = new Label();
            cmbParity = new ComboBox();
            lblStopBits = new Label();
            cmbStopBits = new ComboBox();
            lblHandshake = new Label();
            cmbHandshake = new ComboBox();
            flowLayoutPanelButtons = new FlowLayoutPanel();
            btnOK = new Button();
            btnCancel = new Button();
            tableLayoutPanel1.SuspendLayout();
            flowLayoutPanelButtons.SuspendLayout();
            SuspendLayout();
            //
            // tableLayoutPanel1
            //
            tableLayoutPanel1.ColumnCount = 3;
            tableLayoutPanel1.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 90F));
            tableLayoutPanel1.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100F));
            tableLayoutPanel1.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 80F));
            tableLayoutPanel1.Dock = DockStyle.Top;
            tableLayoutPanel1.Location = new Point(0, 0);
            tableLayoutPanel1.Padding = new Padding(12);
            tableLayoutPanel1.RowCount = 6;
            for (int i = 0; i < 6; i++)
            {
                tableLayoutPanel1.RowStyles.Add(new RowStyle(SizeType.Absolute, 34F));
            }
            tableLayoutPanel1.Size = new Size(380, 228);
            tableLayoutPanel1.TabIndex = 0;

            // Row 0: Port
            lblPort.Text = "포트(Port):";
            lblPort.TextAlign = ContentAlignment.MiddleLeft;
            lblPort.Dock = DockStyle.Fill;
            cmbPort.DropDownStyle = ComboBoxStyle.DropDownList;
            cmbPort.Dock = DockStyle.Fill;
            btnRefreshPorts.Text = "새로고침";
            btnRefreshPorts.Dock = DockStyle.Fill;
            btnRefreshPorts.Click += btnRefreshPorts_Click;
            tableLayoutPanel1.Controls.Add(lblPort, 0, 0);
            tableLayoutPanel1.Controls.Add(cmbPort, 1, 0);
            tableLayoutPanel1.Controls.Add(btnRefreshPorts, 2, 0);

            // Row 1: Baud rate
            lblBaudRate.Text = "속도(Baud Rate):";
            lblBaudRate.TextAlign = ContentAlignment.MiddleLeft;
            lblBaudRate.Dock = DockStyle.Fill;
            cmbBaudRate.DropDownStyle = ComboBoxStyle.DropDown;
            cmbBaudRate.Dock = DockStyle.Fill;
            tableLayoutPanel1.Controls.Add(lblBaudRate, 0, 1);
            tableLayoutPanel1.Controls.Add(cmbBaudRate, 1, 1);
            TableLayoutPanel.SetColumnSpan(cmbBaudRate, 2);

            // Row 2: Data bits
            lblDataBits.Text = "데이터 비트:";
            lblDataBits.TextAlign = ContentAlignment.MiddleLeft;
            lblDataBits.Dock = DockStyle.Fill;
            cmbDataBits.DropDownStyle = ComboBoxStyle.DropDownList;
            cmbDataBits.Dock = DockStyle.Fill;
            tableLayoutPanel1.Controls.Add(lblDataBits, 0, 2);
            tableLayoutPanel1.Controls.Add(cmbDataBits, 1, 2);
            TableLayoutPanel.SetColumnSpan(cmbDataBits, 2);

            // Row 3: Parity
            lblParity.Text = "패리티(Parity):";
            lblParity.TextAlign = ContentAlignment.MiddleLeft;
            lblParity.Dock = DockStyle.Fill;
            cmbParity.DropDownStyle = ComboBoxStyle.DropDownList;
            cmbParity.Dock = DockStyle.Fill;
            tableLayoutPanel1.Controls.Add(lblParity, 0, 3);
            tableLayoutPanel1.Controls.Add(cmbParity, 1, 3);
            TableLayoutPanel.SetColumnSpan(cmbParity, 2);

            // Row 4: Stop bits
            lblStopBits.Text = "정지 비트(Stop Bits):";
            lblStopBits.TextAlign = ContentAlignment.MiddleLeft;
            lblStopBits.Dock = DockStyle.Fill;
            cmbStopBits.DropDownStyle = ComboBoxStyle.DropDownList;
            cmbStopBits.Dock = DockStyle.Fill;
            tableLayoutPanel1.Controls.Add(lblStopBits, 0, 4);
            tableLayoutPanel1.Controls.Add(cmbStopBits, 1, 4);
            TableLayoutPanel.SetColumnSpan(cmbStopBits, 2);

            // Row 5: Handshake
            lblHandshake.Text = "흐름 제어:";
            lblHandshake.TextAlign = ContentAlignment.MiddleLeft;
            lblHandshake.Dock = DockStyle.Fill;
            cmbHandshake.DropDownStyle = ComboBoxStyle.DropDownList;
            cmbHandshake.Dock = DockStyle.Fill;
            tableLayoutPanel1.Controls.Add(lblHandshake, 0, 5);
            tableLayoutPanel1.Controls.Add(cmbHandshake, 1, 5);
            TableLayoutPanel.SetColumnSpan(cmbHandshake, 2);

            //
            // flowLayoutPanelButtons
            //
            flowLayoutPanelButtons.Dock = DockStyle.Bottom;
            flowLayoutPanelButtons.FlowDirection = FlowDirection.RightToLeft;
            flowLayoutPanelButtons.Height = 48;
            flowLayoutPanelButtons.Padding = new Padding(12, 8, 12, 8);
            flowLayoutPanelButtons.Controls.Add(btnCancel);
            flowLayoutPanelButtons.Controls.Add(btnOK);

            //
            // btnOK
            //
            btnOK.Text = "확인";
            btnOK.Size = new Size(90, 30);
            btnOK.DialogResult = DialogResult.OK;
            btnOK.Click += btnOK_Click;

            //
            // btnCancel
            //
            btnCancel.Text = "취소";
            btnCancel.Size = new Size(90, 30);
            btnCancel.DialogResult = DialogResult.Cancel;

            //
            // SettingsForm
            //
            AcceptButton = btnOK;
            CancelButton = btnCancel;
            AutoScaleDimensions = new SizeF(7F, 15F);
            AutoScaleMode = AutoScaleMode.Font;
            ClientSize = new Size(380, 228 + 48);
            Controls.Add(tableLayoutPanel1);
            Controls.Add(flowLayoutPanelButtons);
            FormBorderStyle = FormBorderStyle.FixedDialog;
            MaximizeBox = false;
            MinimizeBox = false;
            StartPosition = FormStartPosition.CenterParent;
            Text = "통신 설정";
            tableLayoutPanel1.ResumeLayout(false);
            flowLayoutPanelButtons.ResumeLayout(false);
            ResumeLayout(false);
        }
    }
}
