using System.Drawing;
using System.Windows.Forms;

namespace UartCommunicator
{
    partial class MainForm
    {
        private System.ComponentModel.IContainer? components = null;

        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
                components.Dispose();
            base.Dispose(disposing);
        }

        // ── Controls ──────────────────────────────────────────
        // Menu
        private MenuStrip menuStrip = null!;
        private ToolStripMenuItem menuFile = null!;
        private ToolStripMenuItem menuSaveLog = null!;
        private ToolStripMenuItem menuExit = null!;
        private ToolStripMenuItem menuEdit = null!;
        private ToolStripMenuItem menuClearReceive = null!;
        private ToolStripMenuItem menuClearSend = null!;
        private ToolStripMenuItem menuView = null!;
        private ToolStripMenuItem menuViewAscii = null!;
        private ToolStripMenuItem menuViewHex = null!;

        // Port settings panel
        private Panel pnlSettings = null!;
        private Label lblPort = null!;
        private ComboBox cmbPort = null!;
        private Button btnRefreshPorts = null!;
        private Label lblBaud = null!;
        private ComboBox cmbBaudRate = null!;
        private Label lblDataBits = null!;
        private ComboBox cmbDataBits = null!;
        private Label lblParity = null!;
        private ComboBox cmbParity = null!;
        private Label lblStopBits = null!;
        private ComboBox cmbStopBits = null!;
        private Button btnConnect = null!;

        // Receive panel
        private GroupBox grpReceive = null!;
        private RichTextBox rtbReceive = null!;

        // Send panel
        private GroupBox grpSend = null!;
        private RichTextBox txtSend = null!;
        private Button btnSend = null!;
        private CheckBox chkNewline = null!;
        private CheckBox chkHexSend = null!;
        private CheckBox chkClearOnSend = null!;
        private CheckBox chkAutoScroll = null!;

        // Status bar
        private StatusStrip statusStrip = null!;
        private ToolStripStatusLabel statusLabel = null!;
        private ToolStripStatusLabel viewModeLabel = null!;

        private void InitializeComponent()
        {
            this.SuspendLayout();

            // ── Form ─────────────────────────────────────────
            this.Text            = "UART Communicator";
            this.Size            = new Size(900, 720);
            this.MinimumSize     = new Size(760, 560);
            this.BackColor       = Color.FromArgb(30, 30, 30);
            this.ForeColor       = Color.WhiteSmoke;
            this.Font            = new Font("Segoe UI", 9f);
            this.StartPosition   = FormStartPosition.CenterScreen;

            // ── Menu strip ───────────────────────────────────
            menuStrip = new MenuStrip { BackColor = Color.FromArgb(45, 45, 48), ForeColor = Color.WhiteSmoke };

            menuFile        = new ToolStripMenuItem("파일(&F)");
            menuSaveLog     = new ToolStripMenuItem("로그 저장...", null, menuSaveLog_Click);
            var sep1        = new ToolStripSeparator();
            menuExit        = new ToolStripMenuItem("종료(&X)", null, menuExit_Click);
            menuFile.DropDownItems.AddRange(new ToolStripItem[] { menuSaveLog, sep1, menuExit });

            menuEdit        = new ToolStripMenuItem("편집(&E)");
            menuClearReceive = new ToolStripMenuItem("수신 창 지우기", null, menuClearReceive_Click);
            menuClearSend   = new ToolStripMenuItem("송신 창 지우기", null, menuClearSend_Click);
            menuEdit.DropDownItems.AddRange(new ToolStripItem[] { menuClearReceive, menuClearSend });

            menuView        = new ToolStripMenuItem("보기(&V)");
            menuViewAscii   = new ToolStripMenuItem("ASCII 모드", null, menuViewAscii_Click) { Checked = true, CheckOnClick = false };
            menuViewHex     = new ToolStripMenuItem("HEX 모드",   null, menuViewHex_Click)   { Checked = false, CheckOnClick = false };
            menuView.DropDownItems.AddRange(new ToolStripItem[] { menuViewAscii, menuViewHex });

            menuStrip.Items.AddRange(new ToolStripItem[] { menuFile, menuEdit, menuView });
            this.Controls.Add(menuStrip);
            this.MainMenuStrip = menuStrip;
            StyleMenuItems(menuStrip);

            // ── Settings panel ───────────────────────────────
            pnlSettings = new Panel
            {
                Dock      = DockStyle.Top,
                Height    = 52,
                BackColor = Color.FromArgb(40, 40, 40),
                Padding   = new Padding(6, 8, 6, 4)
            };

            int x = 6;
            int y = 12;
            int labelW = 46;
            int comboH = 26;

            lblPort = MakeLabel("포트:", x, y);
            pnlSettings.Controls.Add(lblPort);
            x += labelW;

            cmbPort = MakeCombo(x, y - 2, 100, comboH);
            pnlSettings.Controls.Add(cmbPort);
            x += 104;

            btnRefreshPorts = MakeButton("↻", x, y - 2, 30, comboH);
            btnRefreshPorts.Font    = new Font("Segoe UI", 11f, System.Drawing.FontStyle.Bold);
            btnRefreshPorts.Click  += btnRefreshPorts_Click;
            ToolTip tt = new ToolTip();
            tt.SetToolTip(btnRefreshPorts, "포트 목록 새로 고침");
            pnlSettings.Controls.Add(btnRefreshPorts);
            x += 36;

            lblBaud = MakeLabel("전송률:", x, y);
            pnlSettings.Controls.Add(lblBaud);
            x += 50;

            cmbBaudRate = MakeCombo(x, y - 2, 90, comboH);
            pnlSettings.Controls.Add(cmbBaudRate);
            x += 94;

            lblDataBits = MakeLabel("데이터:", x, y);
            pnlSettings.Controls.Add(lblDataBits);
            x += 50;

            cmbDataBits = MakeCombo(x, y - 2, 52, comboH);
            cmbDataBits.Items.AddRange(new object[] { 5, 6, 7, 8 });
            cmbDataBits.SelectedIndex = 3;
            pnlSettings.Controls.Add(cmbDataBits);
            x += 56;

            lblParity = MakeLabel("패리티:", x, y);
            pnlSettings.Controls.Add(lblParity);
            x += 50;

            cmbParity = MakeCombo(x, y - 2, 80, comboH);
            cmbParity.Items.AddRange(new object[] { "None", "Odd", "Even", "Mark", "Space" });
            cmbParity.SelectedIndex = 0;
            pnlSettings.Controls.Add(cmbParity);
            x += 84;

            lblStopBits = MakeLabel("정지:", x, y);
            pnlSettings.Controls.Add(lblStopBits);
            x += 40;

            cmbStopBits = MakeCombo(x, y - 2, 60, comboH);
            cmbStopBits.Items.AddRange(new object[] { "One", "Two", "OnePointFive" });
            cmbStopBits.SelectedIndex = 0;
            pnlSettings.Controls.Add(cmbStopBits);
            x += 66;

            btnConnect = MakeButton("연결", x, y - 2, 90, comboH);
            btnConnect.Font      = new Font("Segoe UI", 9f, System.Drawing.FontStyle.Bold);
            btnConnect.BackColor = Color.MediumSeaGreen;
            btnConnect.Click    += btnConnect_Click;
            pnlSettings.Controls.Add(btnConnect);

            this.Controls.Add(pnlSettings);

            // ── Status strip ─────────────────────────────────
            statusStrip = new StatusStrip { BackColor = Color.FromArgb(45, 45, 48), SizingGrip = false };
            statusLabel  = new ToolStripStatusLabel("연결 안됨") { ForeColor = Color.DarkRed, Spring = true, TextAlign = ContentAlignment.MiddleLeft };
            viewModeLabel = new ToolStripStatusLabel("보기: ASCII") { ForeColor = Color.Gold };
            statusStrip.Items.AddRange(new ToolStripItem[] { statusLabel, viewModeLabel });
            this.Controls.Add(statusStrip);

            // ── Send group ───────────────────────────────────
            grpSend = new GroupBox
            {
                Text      = "송신 데이터",
                Dock      = DockStyle.Bottom,
                Height    = 130,
                ForeColor = Color.WhiteSmoke,
                BackColor = Color.FromArgb(35, 35, 35),
                Padding   = new Padding(6)
            };

            txtSend = new RichTextBox
            {
                Dock      = DockStyle.Fill,
                BackColor = Color.FromArgb(20, 20, 20),
                ForeColor = Color.WhiteSmoke,
                Font      = new Font("Consolas", 10f),
                BorderStyle = BorderStyle.None,
                WordWrap  = true,
                AcceptsTab = true
            };
            txtSend.KeyDown += txtSend_KeyDown;

            var pnlSendBottom = new Panel { Dock = DockStyle.Bottom, Height = 34, BackColor = Color.FromArgb(35, 35, 35) };

            chkNewline    = MakeCheck("\\r\\n 추가", 4, 8);
            chkHexSend    = MakeCheck("HEX 송신", 90, 8);
            chkClearOnSend = MakeCheck("송신 후 지우기", 180, 8);
            chkAutoScroll  = MakeCheck("자동 스크롤", 290, 8);
            chkAutoScroll.Checked = true;

            btnSend = MakeButton("송신  (Enter)", 0, 4, 130, 28);
            btnSend.Dock      = DockStyle.Right;
            btnSend.Font      = new Font("Segoe UI", 9f, System.Drawing.FontStyle.Bold);
            btnSend.BackColor = Color.SteelBlue;
            btnSend.Click    += btnSend_Click;

            pnlSendBottom.Controls.AddRange(new Control[] { chkNewline, chkHexSend, chkClearOnSend, chkAutoScroll, btnSend });

            grpSend.Controls.Add(txtSend);
            grpSend.Controls.Add(pnlSendBottom);
            this.Controls.Add(grpSend);

            // ── Receive group ────────────────────────────────
            grpReceive = new GroupBox
            {
                Text      = "수신 데이터",
                Dock      = DockStyle.Fill,
                ForeColor = Color.WhiteSmoke,
                BackColor = Color.FromArgb(35, 35, 35),
                Padding   = new Padding(6)
            };

            rtbReceive = new RichTextBox
            {
                Dock        = DockStyle.Fill,
                BackColor   = Color.Black,
                ForeColor   = Color.LimeGreen,
                Font        = new Font("Consolas", 10f),
                ReadOnly    = true,
                BorderStyle = BorderStyle.None,
                WordWrap    = false,
                ScrollBars  = RichTextBoxScrollBars.Both
            };

            grpReceive.Controls.Add(rtbReceive);
            this.Controls.Add(grpReceive);

            this.ResumeLayout(false);
            this.PerformLayout();
        }

        // ── UI factory helpers ─────────────────────────────────

        private static Label MakeLabel(string text, int x, int y)
        {
            return new Label
            {
                Text      = text,
                Location  = new Point(x, y),
                AutoSize  = true,
                ForeColor = Color.WhiteSmoke,
                BackColor = Color.Transparent
            };
        }

        private static ComboBox MakeCombo(int x, int y, int w, int h)
        {
            return new ComboBox
            {
                Location         = new Point(x, y),
                Size             = new Size(w, h),
                DropDownStyle    = ComboBoxStyle.DropDownList,
                BackColor        = Color.FromArgb(55, 55, 55),
                ForeColor        = Color.WhiteSmoke,
                FlatStyle        = FlatStyle.Flat
            };
        }

        private static Button MakeButton(string text, int x, int y, int w, int h)
        {
            return new Button
            {
                Text      = text,
                Location  = new Point(x, y),
                Size      = new Size(w, h),
                FlatStyle = FlatStyle.Flat,
                ForeColor = Color.WhiteSmoke,
                BackColor = Color.FromArgb(60, 60, 60),
                Cursor    = Cursors.Hand
            };
        }

        private static CheckBox MakeCheck(string text, int x, int y)
        {
            return new CheckBox
            {
                Text      = text,
                Location  = new Point(x, y),
                AutoSize  = true,
                ForeColor = Color.WhiteSmoke,
                BackColor = Color.Transparent
            };
        }

        private static void StyleMenuItems(MenuStrip ms)
        {
            ms.Renderer = new ToolStripProfessionalRenderer(new DarkColorTable());
        }
    }

    internal class DarkColorTable : ProfessionalColorTable
    {
        public override Color MenuItemSelected          => Color.FromArgb(60, 60, 60);
        public override Color MenuItemSelectedGradientBegin => Color.FromArgb(60, 60, 60);
        public override Color MenuItemSelectedGradientEnd   => Color.FromArgb(60, 60, 60);
        public override Color MenuItemPressedGradientBegin  => Color.FromArgb(70, 70, 70);
        public override Color MenuItemPressedGradientEnd    => Color.FromArgb(70, 70, 70);
        public override Color MenuBorder                    => Color.FromArgb(70, 70, 70);
        public override Color MenuItemBorder                => Color.FromArgb(80, 80, 80);
        public override Color ToolStripDropDownBackground   => Color.FromArgb(45, 45, 48);
        public override Color ImageMarginGradientBegin      => Color.FromArgb(45, 45, 48);
        public override Color ImageMarginGradientMiddle     => Color.FromArgb(45, 45, 48);
        public override Color ImageMarginGradientEnd        => Color.FromArgb(45, 45, 48);
    }
}
