namespace UartCommunicator
{
    partial class MainForm
    {
        /// <summary>
        ///  Required designer variable.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary>
        ///  Clean up any resources being used.
        /// </summary>
        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        #region Windows Form Designer generated code

        /// <summary>
        ///  Required method for Designer support - do not modify
        ///  the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            this.components = new System.ComponentModel.Container();
            this.menuStrip1 = new System.Windows.Forms.MenuStrip();
            this.menuFile = new System.Windows.Forms.ToolStripMenuItem();
            this.menuSaveLog = new System.Windows.Forms.ToolStripMenuItem();
            this.toolStripSeparator1 = new System.Windows.Forms.ToolStripSeparator();
            this.menuExit = new System.Windows.Forms.ToolStripMenuItem();
            this.menuEditMenu = new System.Windows.Forms.ToolStripMenuItem();
            this.menuClearRx = new System.Windows.Forms.ToolStripMenuItem();
            this.menuCopyRx = new System.Windows.Forms.ToolStripMenuItem();
            this.menuViewMenu = new System.Windows.Forms.ToolStripMenuItem();
            this.menuViewAscii = new System.Windows.Forms.ToolStripMenuItem();
            this.menuViewHex = new System.Windows.Forms.ToolStripMenuItem();
            this.menuSendMenu = new System.Windows.Forms.ToolStripMenuItem();
            this.menuSendAscii = new System.Windows.Forms.ToolStripMenuItem();
            this.menuSendHex = new System.Windows.Forms.ToolStripMenuItem();
            this.statusStrip1 = new System.Windows.Forms.StatusStrip();
            this.toolStripStatusConn = new System.Windows.Forms.ToolStripStatusLabel();
            this.toolStripStatusView = new System.Windows.Forms.ToolStripStatusLabel();
            this.toolStripStatusBytes = new System.Windows.Forms.ToolStripStatusLabel();
            this.splitContainer1 = new System.Windows.Forms.SplitContainer();
            this.grpReceive = new System.Windows.Forms.GroupBox();
            this.rtbReceive = new System.Windows.Forms.RichTextBox();
            this.grpSend = new System.Windows.Forms.GroupBox();
            this.tableLayoutSend = new System.Windows.Forms.TableLayoutPanel();
            this.txtSend = new System.Windows.Forms.RichTextBox();
            this.panelSendOptions = new System.Windows.Forms.Panel();
            this.chkCRLF = new System.Windows.Forms.CheckBox();
            this.chkClearSend = new System.Windows.Forms.CheckBox();
            this.chkAutoScroll = new System.Windows.Forms.CheckBox();
            this.btnClearTx = new System.Windows.Forms.Button();
            this.btnSend = new System.Windows.Forms.Button();
            this.panelPort = new System.Windows.Forms.Panel();
            this.tableLayoutPort = new System.Windows.Forms.TableLayoutPanel();
            this.lblPort = new System.Windows.Forms.Label();
            this.cmbPort = new System.Windows.Forms.ComboBox();
            this.btnRefresh = new System.Windows.Forms.Button();
            this.lblBaudRate = new System.Windows.Forms.Label();
            this.cmbBaudRate = new System.Windows.Forms.ComboBox();
            this.lblDataBits = new System.Windows.Forms.Label();
            this.cmbDataBits = new System.Windows.Forms.ComboBox();
            this.lblParity = new System.Windows.Forms.Label();
            this.cmbParity = new System.Windows.Forms.ComboBox();
            this.lblStopBits = new System.Windows.Forms.Label();
            this.cmbStopBits = new System.Windows.Forms.ComboBox();
            this.lblFlowControl = new System.Windows.Forms.Label();
            this.cmbFlowControl = new System.Windows.Forms.ComboBox();
            this.btnConnect = new System.Windows.Forms.Button();
            this.toolTip1 = new System.Windows.Forms.ToolTip(this.components);

            this.menuStrip1.SuspendLayout();
            this.statusStrip1.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.splitContainer1)).BeginInit();
            this.splitContainer1.Panel1.SuspendLayout();
            this.splitContainer1.Panel2.SuspendLayout();
            this.splitContainer1.SuspendLayout();
            this.grpReceive.SuspendLayout();
            this.grpSend.SuspendLayout();
            this.tableLayoutSend.SuspendLayout();
            this.panelSendOptions.SuspendLayout();
            this.panelPort.SuspendLayout();
            this.tableLayoutPort.SuspendLayout();
            this.SuspendLayout();

            // ──────────────────────────────────────────────────────────────────
            // menuStrip1
            // ──────────────────────────────────────────────────────────────────
            this.menuStrip1.BackColor = System.Drawing.Color.FromArgb(45, 45, 48);
            this.menuStrip1.ForeColor = System.Drawing.Color.WhiteSmoke;
            this.menuStrip1.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
                this.menuFile,
                this.menuEditMenu,
                this.menuViewMenu,
                this.menuSendMenu });
            this.menuStrip1.Location = new System.Drawing.Point(0, 0);
            this.menuStrip1.Name = "menuStrip1";
            this.menuStrip1.Padding = new System.Windows.Forms.Padding(6, 2, 0, 2);
            this.menuStrip1.Size = new System.Drawing.Size(980, 24);
            this.menuStrip1.TabStop = false;

            // menuFile
            this.menuFile.DropDownItems.AddRange(new System.Windows.Forms.ToolStripItem[] {
                this.menuSaveLog,
                this.toolStripSeparator1,
                this.menuExit });
            this.menuFile.ForeColor = System.Drawing.Color.WhiteSmoke;
            this.menuFile.Name = "menuFile";
            this.menuFile.Size = new System.Drawing.Size(57, 20);
            this.menuFile.Text = "파일(&F)";

            this.menuSaveLog.Name = "menuSaveLog";
            this.menuSaveLog.ShortcutKeys = System.Windows.Forms.Keys.Control | System.Windows.Forms.Keys.S;
            this.menuSaveLog.Size = new System.Drawing.Size(192, 22);
            this.menuSaveLog.Text = "로그 저장(&S)...";
            this.menuSaveLog.Click += new System.EventHandler(this.menuSaveLog_Click);

            this.toolStripSeparator1.Name = "toolStripSeparator1";
            this.toolStripSeparator1.Size = new System.Drawing.Size(189, 6);

            this.menuExit.Name = "menuExit";
            this.menuExit.ShortcutKeys = System.Windows.Forms.Keys.Alt | System.Windows.Forms.Keys.F4;
            this.menuExit.Size = new System.Drawing.Size(192, 22);
            this.menuExit.Text = "종료(&X)";
            this.menuExit.Click += new System.EventHandler(this.menuExit_Click);

            // menuEditMenu
            this.menuEditMenu.DropDownItems.AddRange(new System.Windows.Forms.ToolStripItem[] {
                this.menuClearRx,
                this.menuCopyRx });
            this.menuEditMenu.ForeColor = System.Drawing.Color.WhiteSmoke;
            this.menuEditMenu.Name = "menuEditMenu";
            this.menuEditMenu.Size = new System.Drawing.Size(57, 20);
            this.menuEditMenu.Text = "편집(&E)";

            this.menuClearRx.Name = "menuClearRx";
            this.menuClearRx.ShortcutKeys = System.Windows.Forms.Keys.Control | System.Windows.Forms.Keys.L;
            this.menuClearRx.Size = new System.Drawing.Size(200, 22);
            this.menuClearRx.Text = "수신 창 지우기(&L)";
            this.menuClearRx.Click += new System.EventHandler(this.menuClearRx_Click);

            this.menuCopyRx.Name = "menuCopyRx";
            this.menuCopyRx.ShortcutKeys = System.Windows.Forms.Keys.Control | System.Windows.Forms.Keys.Shift | System.Windows.Forms.Keys.C;
            this.menuCopyRx.Size = new System.Drawing.Size(200, 22);
            this.menuCopyRx.Text = "수신 내용 복사(&C)";
            this.menuCopyRx.Click += new System.EventHandler(this.menuCopyRx_Click);

            // menuViewMenu
            this.menuViewMenu.DropDownItems.AddRange(new System.Windows.Forms.ToolStripItem[] {
                this.menuViewAscii,
                this.menuViewHex });
            this.menuViewMenu.ForeColor = System.Drawing.Color.WhiteSmoke;
            this.menuViewMenu.Name = "menuViewMenu";
            this.menuViewMenu.Size = new System.Drawing.Size(68, 20);
            this.menuViewMenu.Text = "수신 보기(&V)";

            this.menuViewAscii.Checked = true;
            this.menuViewAscii.CheckOnClick = false;
            this.menuViewAscii.Name = "menuViewAscii";
            this.menuViewAscii.ShortcutKeys = System.Windows.Forms.Keys.Control | System.Windows.Forms.Keys.D1;
            this.menuViewAscii.Size = new System.Drawing.Size(200, 22);
            this.menuViewAscii.Text = "ASCII 모드 (Ctrl+1)";
            this.menuViewAscii.Click += new System.EventHandler(this.menuViewAscii_Click);

            this.menuViewHex.CheckOnClick = false;
            this.menuViewHex.Name = "menuViewHex";
            this.menuViewHex.ShortcutKeys = System.Windows.Forms.Keys.Control | System.Windows.Forms.Keys.D2;
            this.menuViewHex.Size = new System.Drawing.Size(200, 22);
            this.menuViewHex.Text = "HEX 모드  (Ctrl+2)";
            this.menuViewHex.Click += new System.EventHandler(this.menuViewHex_Click);

            // menuSendMenu
            this.menuSendMenu.DropDownItems.AddRange(new System.Windows.Forms.ToolStripItem[] {
                this.menuSendAscii,
                this.menuSendHex });
            this.menuSendMenu.ForeColor = System.Drawing.Color.WhiteSmoke;
            this.menuSendMenu.Name = "menuSendMenu";
            this.menuSendMenu.Size = new System.Drawing.Size(68, 20);
            this.menuSendMenu.Text = "송신 형식(&T)";

            this.menuSendAscii.Checked = true;
            this.menuSendAscii.CheckOnClick = false;
            this.menuSendAscii.Name = "menuSendAscii";
            this.menuSendAscii.Size = new System.Drawing.Size(200, 22);
            this.menuSendAscii.Text = "ASCII 송신 (Ctrl+3)";
            this.menuSendAscii.ShortcutKeys = System.Windows.Forms.Keys.Control | System.Windows.Forms.Keys.D3;
            this.menuSendAscii.Click += new System.EventHandler(this.menuSendAscii_Click);

            this.menuSendHex.CheckOnClick = false;
            this.menuSendHex.Name = "menuSendHex";
            this.menuSendHex.Size = new System.Drawing.Size(200, 22);
            this.menuSendHex.Text = "HEX 송신  (Ctrl+4)";
            this.menuSendHex.ShortcutKeys = System.Windows.Forms.Keys.Control | System.Windows.Forms.Keys.D4;
            this.menuSendHex.Click += new System.EventHandler(this.menuSendHex_Click);

            // ──────────────────────────────────────────────────────────────────
            // statusStrip1
            // ──────────────────────────────────────────────────────────────────
            this.statusStrip1.BackColor = System.Drawing.Color.FromArgb(45, 45, 48);
            this.statusStrip1.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
                this.toolStripStatusConn,
                this.toolStripStatusView,
                this.toolStripStatusBytes });
            this.statusStrip1.Location = new System.Drawing.Point(0, 668);
            this.statusStrip1.Name = "statusStrip1";
            this.statusStrip1.Size = new System.Drawing.Size(980, 22);
            this.statusStrip1.SizingGrip = false;

            this.toolStripStatusConn.AutoSize = false;
            this.toolStripStatusConn.ForeColor = System.Drawing.Color.Tomato;
            this.toolStripStatusConn.Name = "toolStripStatusConn";
            this.toolStripStatusConn.Size = new System.Drawing.Size(480, 17);
            this.toolStripStatusConn.Spring = false;
            this.toolStripStatusConn.Text = "  연결 안됨";
            this.toolStripStatusConn.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;

            this.toolStripStatusView.AutoSize = false;
            this.toolStripStatusView.ForeColor = System.Drawing.Color.Gold;
            this.toolStripStatusView.Name = "toolStripStatusView";
            this.toolStripStatusView.Size = new System.Drawing.Size(160, 17);
            this.toolStripStatusView.Spring = false;
            this.toolStripStatusView.Text = "  보기: ASCII";
            this.toolStripStatusView.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;

            this.toolStripStatusBytes.AutoSize = false;
            this.toolStripStatusBytes.ForeColor = System.Drawing.Color.LightGray;
            this.toolStripStatusBytes.Name = "toolStripStatusBytes";
            this.toolStripStatusBytes.Size = new System.Drawing.Size(200, 17);
            this.toolStripStatusBytes.Spring = true;
            this.toolStripStatusBytes.Text = "  수신: 0 bytes";
            this.toolStripStatusBytes.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;

            // ──────────────────────────────────────────────────────────────────
            // panelPort  (포트 설정 상단 패널)
            // ──────────────────────────────────────────────────────────────────
            this.panelPort.BackColor = System.Drawing.Color.FromArgb(37, 37, 38);
            this.panelPort.Controls.Add(this.tableLayoutPort);
            this.panelPort.Dock = System.Windows.Forms.DockStyle.Top;
            this.panelPort.Location = new System.Drawing.Point(0, 24);
            this.panelPort.Name = "panelPort";
            this.panelPort.Padding = new System.Windows.Forms.Padding(6, 6, 6, 6);
            this.panelPort.Size = new System.Drawing.Size(980, 54);

            // tableLayoutPort
            this.tableLayoutPort.ColumnCount = 14;
            this.tableLayoutPort.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.AutoSize));
            this.tableLayoutPort.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Absolute, 110F));
            this.tableLayoutPort.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Absolute, 32F));
            this.tableLayoutPort.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.AutoSize));
            this.tableLayoutPort.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Absolute, 100F));
            this.tableLayoutPort.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.AutoSize));
            this.tableLayoutPort.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Absolute, 55F));
            this.tableLayoutPort.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.AutoSize));
            this.tableLayoutPort.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Absolute, 90F));
            this.tableLayoutPort.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.AutoSize));
            this.tableLayoutPort.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Absolute, 60F));
            this.tableLayoutPort.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.AutoSize));
            this.tableLayoutPort.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Absolute, 165F));
            this.tableLayoutPort.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Absolute, 110F));
            this.tableLayoutPort.Controls.Add(this.lblPort, 0, 0);
            this.tableLayoutPort.Controls.Add(this.cmbPort, 1, 0);
            this.tableLayoutPort.Controls.Add(this.btnRefresh, 2, 0);
            this.tableLayoutPort.Controls.Add(this.lblBaudRate, 3, 0);
            this.tableLayoutPort.Controls.Add(this.cmbBaudRate, 4, 0);
            this.tableLayoutPort.Controls.Add(this.lblDataBits, 5, 0);
            this.tableLayoutPort.Controls.Add(this.cmbDataBits, 6, 0);
            this.tableLayoutPort.Controls.Add(this.lblParity, 7, 0);
            this.tableLayoutPort.Controls.Add(this.cmbParity, 8, 0);
            this.tableLayoutPort.Controls.Add(this.lblStopBits, 9, 0);
            this.tableLayoutPort.Controls.Add(this.cmbStopBits, 10, 0);
            this.tableLayoutPort.Controls.Add(this.lblFlowControl, 11, 0);
            this.tableLayoutPort.Controls.Add(this.cmbFlowControl, 12, 0);
            this.tableLayoutPort.Controls.Add(this.btnConnect, 13, 0);
            this.tableLayoutPort.Dock = System.Windows.Forms.DockStyle.Fill;
            this.tableLayoutPort.Location = new System.Drawing.Point(6, 6);
            this.tableLayoutPort.Name = "tableLayoutPort";
            this.tableLayoutPort.RowCount = 1;
            this.tableLayoutPort.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 100F));
            this.tableLayoutPort.Size = new System.Drawing.Size(968, 42);

            // lblPort
            this.lblPort.Anchor = System.Windows.Forms.AnchorStyles.Left;
            this.lblPort.AutoSize = true;
            this.lblPort.ForeColor = System.Drawing.Color.WhiteSmoke;
            this.lblPort.Name = "lblPort";
            this.lblPort.Text = "포트:";

            // cmbPort
            this.cmbPort.Anchor = System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right;
            this.cmbPort.BackColor = System.Drawing.Color.FromArgb(60, 60, 60);
            this.cmbPort.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this.cmbPort.FlatStyle = System.Windows.Forms.FlatStyle.Flat;
            this.cmbPort.ForeColor = System.Drawing.Color.WhiteSmoke;
            this.cmbPort.Name = "cmbPort";

            // btnRefresh
            this.btnRefresh.Anchor = System.Windows.Forms.AnchorStyles.Left;
            this.btnRefresh.BackColor = System.Drawing.Color.FromArgb(60, 60, 60);
            this.btnRefresh.FlatStyle = System.Windows.Forms.FlatStyle.Flat;
            this.btnRefresh.ForeColor = System.Drawing.Color.WhiteSmoke;
            this.btnRefresh.Name = "btnRefresh";
            this.btnRefresh.Size = new System.Drawing.Size(28, 26);
            this.btnRefresh.Text = "↻";
            this.btnRefresh.Font = new System.Drawing.Font("Segoe UI", 11F, System.Drawing.FontStyle.Bold);
            this.btnRefresh.Click += new System.EventHandler(this.btnRefresh_Click);
            this.toolTip1.SetToolTip(this.btnRefresh, "포트 목록 새로 고침");

            // lblBaudRate
            this.lblBaudRate.Anchor = System.Windows.Forms.AnchorStyles.Left;
            this.lblBaudRate.AutoSize = true;
            this.lblBaudRate.ForeColor = System.Drawing.Color.WhiteSmoke;
            this.lblBaudRate.Name = "lblBaudRate";
            this.lblBaudRate.Padding = new System.Windows.Forms.Padding(6, 0, 0, 0);
            this.lblBaudRate.Text = "전송률:";

            // cmbBaudRate
            this.cmbBaudRate.Anchor = System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right;
            this.cmbBaudRate.BackColor = System.Drawing.Color.FromArgb(60, 60, 60);
            this.cmbBaudRate.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this.cmbBaudRate.FlatStyle = System.Windows.Forms.FlatStyle.Flat;
            this.cmbBaudRate.ForeColor = System.Drawing.Color.WhiteSmoke;
            this.cmbBaudRate.Name = "cmbBaudRate";

            // lblDataBits
            this.lblDataBits.Anchor = System.Windows.Forms.AnchorStyles.Left;
            this.lblDataBits.AutoSize = true;
            this.lblDataBits.ForeColor = System.Drawing.Color.WhiteSmoke;
            this.lblDataBits.Name = "lblDataBits";
            this.lblDataBits.Padding = new System.Windows.Forms.Padding(6, 0, 0, 0);
            this.lblDataBits.Text = "데이터:";

            // cmbDataBits
            this.cmbDataBits.Anchor = System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right;
            this.cmbDataBits.BackColor = System.Drawing.Color.FromArgb(60, 60, 60);
            this.cmbDataBits.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this.cmbDataBits.FlatStyle = System.Windows.Forms.FlatStyle.Flat;
            this.cmbDataBits.ForeColor = System.Drawing.Color.WhiteSmoke;
            this.cmbDataBits.Name = "cmbDataBits";

            // lblParity
            this.lblParity.Anchor = System.Windows.Forms.AnchorStyles.Left;
            this.lblParity.AutoSize = true;
            this.lblParity.ForeColor = System.Drawing.Color.WhiteSmoke;
            this.lblParity.Name = "lblParity";
            this.lblParity.Padding = new System.Windows.Forms.Padding(6, 0, 0, 0);
            this.lblParity.Text = "패리티:";

            // cmbParity
            this.cmbParity.Anchor = System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right;
            this.cmbParity.BackColor = System.Drawing.Color.FromArgb(60, 60, 60);
            this.cmbParity.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this.cmbParity.FlatStyle = System.Windows.Forms.FlatStyle.Flat;
            this.cmbParity.ForeColor = System.Drawing.Color.WhiteSmoke;
            this.cmbParity.Name = "cmbParity";

            // lblStopBits
            this.lblStopBits.Anchor = System.Windows.Forms.AnchorStyles.Left;
            this.lblStopBits.AutoSize = true;
            this.lblStopBits.ForeColor = System.Drawing.Color.WhiteSmoke;
            this.lblStopBits.Name = "lblStopBits";
            this.lblStopBits.Padding = new System.Windows.Forms.Padding(6, 0, 0, 0);
            this.lblStopBits.Text = "정지:";

            // cmbStopBits
            this.cmbStopBits.Anchor = System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right;
            this.cmbStopBits.BackColor = System.Drawing.Color.FromArgb(60, 60, 60);
            this.cmbStopBits.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this.cmbStopBits.FlatStyle = System.Windows.Forms.FlatStyle.Flat;
            this.cmbStopBits.ForeColor = System.Drawing.Color.WhiteSmoke;
            this.cmbStopBits.Name = "cmbStopBits";

            // lblFlowControl
            this.lblFlowControl.Anchor = System.Windows.Forms.AnchorStyles.Left;
            this.lblFlowControl.AutoSize = true;
            this.lblFlowControl.ForeColor = System.Drawing.Color.WhiteSmoke;
            this.lblFlowControl.Name = "lblFlowControl";
            this.lblFlowControl.Padding = new System.Windows.Forms.Padding(6, 0, 0, 0);
            this.lblFlowControl.Text = "흐름 제어:";

            // cmbFlowControl
            this.cmbFlowControl.Anchor = System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right;
            this.cmbFlowControl.BackColor = System.Drawing.Color.FromArgb(60, 60, 60);
            this.cmbFlowControl.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this.cmbFlowControl.FlatStyle = System.Windows.Forms.FlatStyle.Flat;
            this.cmbFlowControl.ForeColor = System.Drawing.Color.WhiteSmoke;
            this.cmbFlowControl.Name = "cmbFlowControl";

            // btnConnect
            this.btnConnect.Anchor = System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right;
            this.btnConnect.BackColor = System.Drawing.Color.FromArgb(39, 174, 96);
            this.btnConnect.FlatStyle = System.Windows.Forms.FlatStyle.Flat;
            this.btnConnect.Font = new System.Drawing.Font("Segoe UI", 9F, System.Drawing.FontStyle.Bold);
            this.btnConnect.ForeColor = System.Drawing.Color.White;
            this.btnConnect.Name = "btnConnect";
            this.btnConnect.Size = new System.Drawing.Size(106, 28);
            this.btnConnect.Text = "연결 (F5)";
            this.btnConnect.Click += new System.EventHandler(this.btnConnect_Click);

            // ──────────────────────────────────────────────────────────────────
            // splitContainer1  (수신 위 / 송신 아래)
            // ──────────────────────────────────────────────────────────────────
            this.splitContainer1.Dock = System.Windows.Forms.DockStyle.Fill;
            this.splitContainer1.Location = new System.Drawing.Point(0, 78);
            this.splitContainer1.Name = "splitContainer1";
            this.splitContainer1.Orientation = System.Windows.Forms.Orientation.Horizontal;
            this.splitContainer1.Size = new System.Drawing.Size(980, 590);
            this.splitContainer1.SplitterDistance = 390;
            this.splitContainer1.SplitterWidth = 5;
            this.splitContainer1.Panel1MinSize = 100;
            this.splitContainer1.Panel2MinSize = 120;

            // splitContainer1.Panel1 – 수신
            this.splitContainer1.Panel1.Controls.Add(this.grpReceive);
            this.splitContainer1.Panel1.Padding = new System.Windows.Forms.Padding(6, 4, 6, 2);

            // splitContainer1.Panel2 – 송신
            this.splitContainer1.Panel2.Controls.Add(this.grpSend);
            this.splitContainer1.Panel2.Padding = new System.Windows.Forms.Padding(6, 2, 6, 4);

            // grpReceive
            this.grpReceive.Controls.Add(this.rtbReceive);
            this.grpReceive.Dock = System.Windows.Forms.DockStyle.Fill;
            this.grpReceive.ForeColor = System.Drawing.Color.WhiteSmoke;
            this.grpReceive.Name = "grpReceive";
            this.grpReceive.Padding = new System.Windows.Forms.Padding(4);
            this.grpReceive.Text = "수신 데이터";

            // rtbReceive
            this.rtbReceive.BackColor = System.Drawing.Color.Black;
            this.rtbReceive.BorderStyle = System.Windows.Forms.BorderStyle.None;
            this.rtbReceive.Dock = System.Windows.Forms.DockStyle.Fill;
            this.rtbReceive.Font = new System.Drawing.Font("Consolas", 10F);
            this.rtbReceive.ForeColor = System.Drawing.Color.FromArgb(50, 215, 100);
            this.rtbReceive.Name = "rtbReceive";
            this.rtbReceive.ReadOnly = true;
            this.rtbReceive.ScrollBars = System.Windows.Forms.RichTextBoxScrollBars.Both;
            this.rtbReceive.WordWrap = false;

            // grpSend
            this.grpSend.Controls.Add(this.tableLayoutSend);
            this.grpSend.Dock = System.Windows.Forms.DockStyle.Fill;
            this.grpSend.ForeColor = System.Drawing.Color.WhiteSmoke;
            this.grpSend.Name = "grpSend";
            this.grpSend.Padding = new System.Windows.Forms.Padding(4);
            this.grpSend.Text = "송신 데이터  (ASCII)";

            // tableLayoutSend
            this.tableLayoutSend.ColumnCount = 1;
            this.tableLayoutSend.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
            this.tableLayoutSend.Controls.Add(this.txtSend, 0, 0);
            this.tableLayoutSend.Controls.Add(this.panelSendOptions, 0, 1);
            this.tableLayoutSend.Dock = System.Windows.Forms.DockStyle.Fill;
            this.tableLayoutSend.Name = "tableLayoutSend";
            this.tableLayoutSend.RowCount = 2;
            this.tableLayoutSend.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 100F));
            this.tableLayoutSend.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Absolute, 36F));

            // txtSend
            this.txtSend.BackColor = System.Drawing.Color.FromArgb(20, 20, 20);
            this.txtSend.BorderStyle = System.Windows.Forms.BorderStyle.None;
            this.txtSend.Dock = System.Windows.Forms.DockStyle.Fill;
            this.txtSend.Font = new System.Drawing.Font("Consolas", 10F);
            this.txtSend.ForeColor = System.Drawing.Color.WhiteSmoke;
            this.txtSend.Name = "txtSend";
            this.txtSend.WordWrap = true;
            this.txtSend.KeyDown += new System.Windows.Forms.KeyEventHandler(this.txtSend_KeyDown);

            // panelSendOptions
            this.panelSendOptions.BackColor = System.Drawing.Color.FromArgb(37, 37, 38);
            this.panelSendOptions.Controls.Add(this.chkCRLF);
            this.panelSendOptions.Controls.Add(this.chkClearSend);
            this.panelSendOptions.Controls.Add(this.chkAutoScroll);
            this.panelSendOptions.Controls.Add(this.btnClearTx);
            this.panelSendOptions.Controls.Add(this.btnSend);
            this.panelSendOptions.Dock = System.Windows.Forms.DockStyle.Fill;
            this.panelSendOptions.Name = "panelSendOptions";

            // chkCRLF
            this.chkCRLF.AutoSize = true;
            this.chkCRLF.ForeColor = System.Drawing.Color.WhiteSmoke;
            this.chkCRLF.Location = new System.Drawing.Point(4, 8);
            this.chkCRLF.Name = "chkCRLF";
            this.chkCRLF.Text = "\\r\\n 추가";

            // chkClearSend
            this.chkClearSend.AutoSize = true;
            this.chkClearSend.ForeColor = System.Drawing.Color.WhiteSmoke;
            this.chkClearSend.Location = new System.Drawing.Point(100, 8);
            this.chkClearSend.Name = "chkClearSend";
            this.chkClearSend.Text = "송신 후 지우기";

            // chkAutoScroll
            this.chkAutoScroll.AutoSize = true;
            this.chkAutoScroll.Checked = true;
            this.chkAutoScroll.CheckState = System.Windows.Forms.CheckState.Checked;
            this.chkAutoScroll.ForeColor = System.Drawing.Color.WhiteSmoke;
            this.chkAutoScroll.Location = new System.Drawing.Point(215, 8);
            this.chkAutoScroll.Name = "chkAutoScroll";
            this.chkAutoScroll.Text = "자동 스크롤";

            // btnClearTx
            this.btnClearTx.Anchor = System.Windows.Forms.AnchorStyles.Right;
            this.btnClearTx.BackColor = System.Drawing.Color.FromArgb(60, 60, 60);
            this.btnClearTx.FlatStyle = System.Windows.Forms.FlatStyle.Flat;
            this.btnClearTx.ForeColor = System.Drawing.Color.WhiteSmoke;
            this.btnClearTx.Location = new System.Drawing.Point(740, 4);
            this.btnClearTx.Name = "btnClearTx";
            this.btnClearTx.Size = new System.Drawing.Size(90, 28);
            this.btnClearTx.Text = "창 지우기";
            this.btnClearTx.Click += new System.EventHandler(this.btnClearTx_Click);

            // btnSend
            this.btnSend.Anchor = System.Windows.Forms.AnchorStyles.Right;
            this.btnSend.BackColor = System.Drawing.Color.FromArgb(0, 122, 204);
            this.btnSend.FlatStyle = System.Windows.Forms.FlatStyle.Flat;
            this.btnSend.Font = new System.Drawing.Font("Segoe UI", 9F, System.Drawing.FontStyle.Bold);
            this.btnSend.ForeColor = System.Drawing.Color.White;
            this.btnSend.Location = new System.Drawing.Point(836, 4);
            this.btnSend.Name = "btnSend";
            this.btnSend.Size = new System.Drawing.Size(120, 28);
            this.btnSend.Text = "송신  [Enter]";
            this.btnSend.Click += new System.EventHandler(this.btnSend_Click);

            // ──────────────────────────────────────────────────────────────────
            // MainForm
            // ──────────────────────────────────────────────────────────────────
            this.AutoScaleDimensions = new System.Drawing.SizeF(7F, 15F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.BackColor = System.Drawing.Color.FromArgb(30, 30, 30);
            this.ClientSize = new System.Drawing.Size(980, 690);
            this.Controls.Add(this.splitContainer1);
            this.Controls.Add(this.panelPort);
            this.Controls.Add(this.statusStrip1);
            this.Controls.Add(this.menuStrip1);
            this.Font = new System.Drawing.Font("Segoe UI", 9F);
            this.ForeColor = System.Drawing.Color.WhiteSmoke;
            this.MainMenuStrip = this.menuStrip1;
            this.MinimumSize = new System.Drawing.Size(820, 580);
            this.Name = "MainForm";
            this.StartPosition = System.Windows.Forms.FormStartPosition.CenterScreen;
            this.Text = "UART Communicator";

            this.menuStrip1.ResumeLayout(false);
            this.menuStrip1.PerformLayout();
            this.statusStrip1.ResumeLayout(false);
            this.statusStrip1.PerformLayout();
            this.splitContainer1.Panel1.ResumeLayout(false);
            this.splitContainer1.Panel2.ResumeLayout(false);
            ((System.ComponentModel.ISupportInitialize)(this.splitContainer1)).EndInit();
            this.splitContainer1.ResumeLayout(false);
            this.grpReceive.ResumeLayout(false);
            this.grpSend.ResumeLayout(false);
            this.tableLayoutSend.ResumeLayout(false);
            this.panelSendOptions.ResumeLayout(false);
            this.panelSendOptions.PerformLayout();
            this.panelPort.ResumeLayout(false);
            this.tableLayoutPort.ResumeLayout(false);
            this.tableLayoutPort.PerformLayout();
            this.ResumeLayout(false);
            this.PerformLayout();
        }

        #endregion

        // ── Controls 선언 ─────────────────────────────────────────────────────
        private System.Windows.Forms.MenuStrip menuStrip1;
        private System.Windows.Forms.ToolStripMenuItem menuFile;
        private System.Windows.Forms.ToolStripMenuItem menuSaveLog;
        private System.Windows.Forms.ToolStripSeparator toolStripSeparator1;
        private System.Windows.Forms.ToolStripMenuItem menuExit;
        private System.Windows.Forms.ToolStripMenuItem menuEditMenu;
        private System.Windows.Forms.ToolStripMenuItem menuClearRx;
        private System.Windows.Forms.ToolStripMenuItem menuCopyRx;
        private System.Windows.Forms.ToolStripMenuItem menuViewMenu;
        private System.Windows.Forms.ToolStripMenuItem menuViewAscii;
        private System.Windows.Forms.ToolStripMenuItem menuViewHex;
        private System.Windows.Forms.ToolStripMenuItem menuSendMenu;
        private System.Windows.Forms.ToolStripMenuItem menuSendAscii;
        private System.Windows.Forms.ToolStripMenuItem menuSendHex;
        private System.Windows.Forms.StatusStrip statusStrip1;
        private System.Windows.Forms.ToolStripStatusLabel toolStripStatusConn;
        private System.Windows.Forms.ToolStripStatusLabel toolStripStatusView;
        private System.Windows.Forms.ToolStripStatusLabel toolStripStatusBytes;
        private System.Windows.Forms.SplitContainer splitContainer1;
        private System.Windows.Forms.GroupBox grpReceive;
        private System.Windows.Forms.RichTextBox rtbReceive;
        private System.Windows.Forms.GroupBox grpSend;
        private System.Windows.Forms.TableLayoutPanel tableLayoutSend;
        private System.Windows.Forms.RichTextBox txtSend;
        private System.Windows.Forms.Panel panelSendOptions;
        private System.Windows.Forms.CheckBox chkCRLF;
        private System.Windows.Forms.CheckBox chkClearSend;
        private System.Windows.Forms.CheckBox chkAutoScroll;
        private System.Windows.Forms.Button btnClearTx;
        private System.Windows.Forms.Button btnSend;
        private System.Windows.Forms.Panel panelPort;
        private System.Windows.Forms.TableLayoutPanel tableLayoutPort;
        private System.Windows.Forms.Label lblPort;
        private System.Windows.Forms.ComboBox cmbPort;
        private System.Windows.Forms.Button btnRefresh;
        private System.Windows.Forms.Label lblBaudRate;
        private System.Windows.Forms.ComboBox cmbBaudRate;
        private System.Windows.Forms.Label lblDataBits;
        private System.Windows.Forms.ComboBox cmbDataBits;
        private System.Windows.Forms.Label lblParity;
        private System.Windows.Forms.ComboBox cmbParity;
        private System.Windows.Forms.Label lblStopBits;
        private System.Windows.Forms.ComboBox cmbStopBits;
        private System.Windows.Forms.Label lblFlowControl;
        private System.Windows.Forms.ComboBox cmbFlowControl;
        private System.Windows.Forms.Button btnConnect;
        private System.Windows.Forms.ToolTip toolTip1;
    }
}
