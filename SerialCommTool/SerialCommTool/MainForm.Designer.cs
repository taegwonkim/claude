namespace SerialCommTool
{
    partial class MainForm
    {
        private System.ComponentModel.IContainer components = null;

        private MenuStrip menuStrip1;
        private ToolStripMenuItem miFile;
        private ToolStripMenuItem miExit;
        private ToolStripMenuItem miComm;
        private ToolStripMenuItem miSettings;

        private Panel panelTop;
        private Label lblSettingsInfo;
        private FlowLayoutPanel flowLayoutPanelTopButtons;
        private Button btnConnect;
        private Button btnSettings;

        private SplitContainer splitContainer1;

        private GroupBox groupReceive;
        private TextBox txtReceive;
        private Panel panelReceiveBottom;
        private CheckBox chkHexView;
        private Button btnClearReceive;

        private GroupBox groupSend;
        private TextBox txtSend;
        private Panel panelSendBottom;
        private FlowLayoutPanel flowLayoutPanelSendOptions;
        private CheckBox chkHexSend;
        private CheckBox chkAppendNewLine;
        private Button btnSend;

        private StatusStrip statusStrip1;
        private ToolStripStatusLabel toolStripStatusLabelConnection;

        private System.IO.Ports.SerialPort serialPort1;

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
            components = new System.ComponentModel.Container();
            serialPort1 = new System.IO.Ports.SerialPort(components);
            serialPort1.DataReceived += serialPort1_DataReceived;

            menuStrip1 = new MenuStrip();
            miFile = new ToolStripMenuItem();
            miExit = new ToolStripMenuItem();
            miComm = new ToolStripMenuItem();
            miSettings = new ToolStripMenuItem();

            panelTop = new Panel();
            lblSettingsInfo = new Label();
            flowLayoutPanelTopButtons = new FlowLayoutPanel();
            btnConnect = new Button();
            btnSettings = new Button();

            splitContainer1 = new SplitContainer();

            groupReceive = new GroupBox();
            txtReceive = new TextBox();
            panelReceiveBottom = new Panel();
            chkHexView = new CheckBox();
            btnClearReceive = new Button();

            groupSend = new GroupBox();
            txtSend = new TextBox();
            panelSendBottom = new Panel();
            flowLayoutPanelSendOptions = new FlowLayoutPanel();
            chkHexSend = new CheckBox();
            chkAppendNewLine = new CheckBox();
            btnSend = new Button();

            statusStrip1 = new StatusStrip();
            toolStripStatusLabelConnection = new ToolStripStatusLabel();

            menuStrip1.SuspendLayout();
            panelTop.SuspendLayout();
            flowLayoutPanelTopButtons.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)splitContainer1).BeginInit();
            splitContainer1.Panel1.SuspendLayout();
            splitContainer1.Panel2.SuspendLayout();
            splitContainer1.SuspendLayout();
            groupReceive.SuspendLayout();
            panelReceiveBottom.SuspendLayout();
            groupSend.SuspendLayout();
            panelSendBottom.SuspendLayout();
            flowLayoutPanelSendOptions.SuspendLayout();
            statusStrip1.SuspendLayout();
            SuspendLayout();

            //
            // menuStrip1
            //
            miExit.Text = "종료(&X)";
            miExit.Click += miExit_Click;
            miFile.Text = "파일(&F)";
            miFile.DropDownItems.Add(miExit);

            miSettings.Text = "통신 설정...(&S)";
            miSettings.Click += btnSettings_Click;
            miComm.Text = "통신(&C)";
            miComm.DropDownItems.Add(miSettings);

            menuStrip1.Items.Add(miFile);
            menuStrip1.Items.Add(miComm);
            menuStrip1.Dock = DockStyle.Top;

            //
            // panelTop
            //
            panelTop.Dock = DockStyle.Top;
            panelTop.Height = 44;
            panelTop.Padding = new Padding(8, 6, 8, 6);

            lblSettingsInfo.Dock = DockStyle.Fill;
            lblSettingsInfo.TextAlign = ContentAlignment.MiddleLeft;
            lblSettingsInfo.AutoEllipsis = true;

            btnConnect.Text = "연결";
            btnConnect.Size = new Size(100, 30);
            btnConnect.Margin = new Padding(4, 0, 0, 0);
            btnConnect.Click += btnConnect_Click;

            btnSettings.Text = "설정...";
            btnSettings.Size = new Size(100, 30);
            btnSettings.Margin = new Padding(4, 0, 0, 0);
            btnSettings.Click += btnSettings_Click;

            flowLayoutPanelTopButtons.Dock = DockStyle.Right;
            flowLayoutPanelTopButtons.FlowDirection = FlowDirection.RightToLeft;
            flowLayoutPanelTopButtons.AutoSize = true;
            flowLayoutPanelTopButtons.WrapContents = false;
            flowLayoutPanelTopButtons.Controls.Add(btnConnect);
            flowLayoutPanelTopButtons.Controls.Add(btnSettings);

            panelTop.Controls.Add(lblSettingsInfo);
            panelTop.Controls.Add(flowLayoutPanelTopButtons);

            //
            // splitContainer1
            //
            splitContainer1.Dock = DockStyle.Fill;
            splitContainer1.Orientation = Orientation.Horizontal;
            splitContainer1.Size = new Size(720, 434);

            //
            // groupReceive (splitContainer1.Panel1)
            //
            groupReceive.Text = "수신 데이터";
            groupReceive.Dock = DockStyle.Fill;

            txtReceive.Multiline = true;
            txtReceive.ReadOnly = true;
            txtReceive.ScrollBars = ScrollBars.Vertical;
            txtReceive.Dock = DockStyle.Fill;
            txtReceive.Font = new Font("Consolas", 9.75F);
            txtReceive.BackColor = Color.White;

            panelReceiveBottom.Dock = DockStyle.Bottom;
            panelReceiveBottom.Height = 36;
            panelReceiveBottom.Padding = new Padding(6);

            chkHexView.Text = "16진수(HEX)로 표시";
            chkHexView.AutoSize = true;
            chkHexView.Dock = DockStyle.Left;

            btnClearReceive.Text = "지우기";
            btnClearReceive.Size = new Size(90, 24);
            btnClearReceive.Dock = DockStyle.Right;
            btnClearReceive.Click += btnClearReceive_Click;

            panelReceiveBottom.Controls.Add(chkHexView);
            panelReceiveBottom.Controls.Add(btnClearReceive);

            groupReceive.Controls.Add(txtReceive);
            groupReceive.Controls.Add(panelReceiveBottom);
            splitContainer1.Panel1.Controls.Add(groupReceive);

            //
            // groupSend (splitContainer1.Panel2)
            //
            groupSend.Text = "송신 데이터";
            groupSend.Dock = DockStyle.Fill;

            txtSend.Multiline = true;
            txtSend.ScrollBars = ScrollBars.Vertical;
            txtSend.Dock = DockStyle.Fill;
            txtSend.Font = new Font("Consolas", 9.75F);
            txtSend.KeyDown += txtSend_KeyDown;

            panelSendBottom.Dock = DockStyle.Bottom;
            panelSendBottom.Height = 40;
            panelSendBottom.Padding = new Padding(6);

            chkHexSend.Text = "16진수(HEX)로 송신";
            chkHexSend.AutoSize = true;

            chkAppendNewLine.Text = "개행 문자 추가(CR+LF)";
            chkAppendNewLine.AutoSize = true;
            chkAppendNewLine.Checked = true;
            chkAppendNewLine.Margin = new Padding(12, 3, 0, 3);

            flowLayoutPanelSendOptions.Dock = DockStyle.Left;
            flowLayoutPanelSendOptions.AutoSize = true;
            flowLayoutPanelSendOptions.WrapContents = false;
            flowLayoutPanelSendOptions.Controls.Add(chkHexSend);
            flowLayoutPanelSendOptions.Controls.Add(chkAppendNewLine);

            btnSend.Text = "보내기";
            btnSend.Size = new Size(100, 28);
            btnSend.Dock = DockStyle.Right;
            btnSend.Click += btnSend_Click;

            panelSendBottom.Controls.Add(flowLayoutPanelSendOptions);
            panelSendBottom.Controls.Add(btnSend);

            groupSend.Controls.Add(txtSend);
            groupSend.Controls.Add(panelSendBottom);
            splitContainer1.Panel2.Controls.Add(groupSend);

            splitContainer1.SplitterDistance = 260;

            //
            // statusStrip1
            //
            toolStripStatusLabelConnection.Text = "연결 안됨";
            toolStripStatusLabelConnection.Spring = true;
            toolStripStatusLabelConnection.TextAlign = ContentAlignment.MiddleLeft;
            statusStrip1.Items.Add(toolStripStatusLabelConnection);
            statusStrip1.Dock = DockStyle.Bottom;

            //
            // MainForm
            //
            AutoScaleDimensions = new SizeF(7F, 15F);
            AutoScaleMode = AutoScaleMode.Font;
            ClientSize = new Size(720, 520);
            Controls.Add(splitContainer1);
            Controls.Add(panelTop);
            Controls.Add(statusStrip1);
            Controls.Add(menuStrip1);
            MainMenuStrip = menuStrip1;
            MinimumSize = new Size(480, 400);
            Text = "시리얼 통신 프로그램";
            FormClosing += MainForm_FormClosing;

            menuStrip1.ResumeLayout(false);
            menuStrip1.PerformLayout();
            flowLayoutPanelTopButtons.ResumeLayout(false);
            panelTop.ResumeLayout(false);
            panelReceiveBottom.ResumeLayout(false);
            panelReceiveBottom.PerformLayout();
            groupReceive.ResumeLayout(false);
            flowLayoutPanelSendOptions.ResumeLayout(false);
            flowLayoutPanelSendOptions.PerformLayout();
            panelSendBottom.ResumeLayout(false);
            panelSendBottom.PerformLayout();
            groupSend.ResumeLayout(false);
            splitContainer1.Panel1.ResumeLayout(false);
            splitContainer1.Panel2.ResumeLayout(false);
            ((System.ComponentModel.ISupportInitialize)splitContainer1).EndInit();
            splitContainer1.ResumeLayout(false);
            statusStrip1.ResumeLayout(false);
            statusStrip1.PerformLayout();
            ResumeLayout(false);
            PerformLayout();
        }
    }
}
