namespace Stm32WifiConfigTool.Panels
{
    partial class EspStatusPanel
    {
        /// <summary>Required designer variable.</summary>
        private System.ComponentModel.IContainer components = null;

        private System.Windows.Forms.TableLayoutPanel _root;
        private System.Windows.Forms.GroupBox _channelGroup;
        private System.Windows.Forms.RadioButton _showUsb;
        private System.Windows.Forms.RadioButton _showUart;
        private System.Windows.Forms.RadioButton _showBoth;
        private System.Windows.Forms.GroupBox _currentGroup;
        private System.Windows.Forms.Label _currentStatusLabel;
        private System.Windows.Forms.Label _lastUpdateLabel;
        private System.Windows.Forms.Button _clearButton;
        private System.Windows.Forms.Label _logLabel;
        private System.Windows.Forms.TextBox _logBox;

        #region Component Designer generated code

        /// <summary>
        /// Required method for Designer support - do not modify
        /// the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            this._root = new System.Windows.Forms.TableLayoutPanel();
            this._channelGroup = new System.Windows.Forms.GroupBox();
            this._showUsb = new System.Windows.Forms.RadioButton();
            this._showUart = new System.Windows.Forms.RadioButton();
            this._showBoth = new System.Windows.Forms.RadioButton();
            this._currentGroup = new System.Windows.Forms.GroupBox();
            this._currentStatusLabel = new System.Windows.Forms.Label();
            this._lastUpdateLabel = new System.Windows.Forms.Label();
            this._clearButton = new System.Windows.Forms.Button();
            this._logLabel = new System.Windows.Forms.Label();
            this._logBox = new System.Windows.Forms.TextBox();
            this._root.SuspendLayout();
            this._channelGroup.SuspendLayout();
            this._currentGroup.SuspendLayout();
            this.SuspendLayout();
            //
            // _root
            //
            this._root.ColumnCount = 1;
            this._root.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
            this._root.Controls.Add(this._channelGroup, 0, 0);
            this._root.Controls.Add(this._currentGroup, 0, 1);
            this._root.Controls.Add(this._clearButton, 0, 2);
            this._root.Controls.Add(this._logLabel, 0, 3);
            this._root.Controls.Add(this._logBox, 0, 4);
            this._root.Dock = System.Windows.Forms.DockStyle.Fill;
            this._root.Location = new System.Drawing.Point(0, 0);
            this._root.Name = "_root";
            this._root.Padding = new System.Windows.Forms.Padding(6);
            this._root.RowCount = 5;
            this._root.RowStyles.Add(new System.Windows.Forms.RowStyle());
            this._root.RowStyles.Add(new System.Windows.Forms.RowStyle());
            this._root.RowStyles.Add(new System.Windows.Forms.RowStyle());
            this._root.RowStyles.Add(new System.Windows.Forms.RowStyle());
            this._root.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 100F));
            this._root.Size = new System.Drawing.Size(300, 520);
            this._root.TabIndex = 0;
            //
            // _channelGroup
            //
            this._channelGroup.Controls.Add(this._showUsb);
            this._channelGroup.Controls.Add(this._showUart);
            this._channelGroup.Controls.Add(this._showBoth);
            this._channelGroup.Dock = System.Windows.Forms.DockStyle.Top;
            this._channelGroup.Location = new System.Drawing.Point(9, 9);
            this._channelGroup.Name = "_channelGroup";
            this._channelGroup.Size = new System.Drawing.Size(282, 50);
            this._channelGroup.TabIndex = 0;
            this._channelGroup.TabStop = false;
            this._channelGroup.Text = "표시 채널";
            //
            // _showUsb
            //
            this._showUsb.AutoSize = true;
            this._showUsb.Location = new System.Drawing.Point(10, 20);
            this._showUsb.Name = "_showUsb";
            this._showUsb.Size = new System.Drawing.Size(48, 19);
            this._showUsb.TabIndex = 0;
            this._showUsb.Text = "USB";
            this._showUsb.CheckedChanged += new System.EventHandler(this.ShowUsb_CheckedChanged);
            //
            // _showUart
            //
            this._showUart.AutoSize = true;
            this._showUart.Location = new System.Drawing.Point(70, 20);
            this._showUart.Name = "_showUart";
            this._showUart.Size = new System.Drawing.Size(52, 19);
            this._showUart.TabIndex = 1;
            this._showUart.Text = "UART";
            this._showUart.CheckedChanged += new System.EventHandler(this.ShowUart_CheckedChanged);
            //
            // _showBoth
            //
            this._showBoth.AutoSize = true;
            this._showBoth.Checked = true;
            this._showBoth.Location = new System.Drawing.Point(140, 20);
            this._showBoth.Name = "_showBoth";
            this._showBoth.Size = new System.Drawing.Size(53, 19);
            this._showBoth.TabIndex = 2;
            this._showBoth.TabStop = true;
            this._showBoth.Text = "둘 다";
            this._showBoth.CheckedChanged += new System.EventHandler(this.ShowBoth_CheckedChanged);
            //
            // _currentGroup
            //
            this._currentGroup.Controls.Add(this._currentStatusLabel);
            this._currentGroup.Controls.Add(this._lastUpdateLabel);
            this._currentGroup.Dock = System.Windows.Forms.DockStyle.Top;
            this._currentGroup.Location = new System.Drawing.Point(9, 59);
            this._currentGroup.Name = "_currentGroup";
            this._currentGroup.Size = new System.Drawing.Size(282, 90);
            this._currentGroup.TabIndex = 1;
            this._currentGroup.TabStop = false;
            this._currentGroup.Text = "현재 ESP32 상태";
            //
            // _currentStatusLabel
            //
            this._currentStatusLabel.AutoSize = true;
            this._currentStatusLabel.Font = new System.Drawing.Font("Segoe UI", 16F, System.Drawing.FontStyle.Bold);
            this._currentStatusLabel.ForeColor = System.Drawing.Color.Gray;
            this._currentStatusLabel.Location = new System.Drawing.Point(10, 20);
            this._currentStatusLabel.Name = "_currentStatusLabel";
            this._currentStatusLabel.Size = new System.Drawing.Size(24, 30);
            this._currentStatusLabel.TabIndex = 0;
            this._currentStatusLabel.Text = "-";
            //
            // _lastUpdateLabel
            //
            this._lastUpdateLabel.AutoSize = true;
            this._lastUpdateLabel.ForeColor = System.Drawing.Color.DimGray;
            this._lastUpdateLabel.Location = new System.Drawing.Point(10, 58);
            this._lastUpdateLabel.Name = "_lastUpdateLabel";
            this._lastUpdateLabel.Size = new System.Drawing.Size(84, 15);
            this._lastUpdateLabel.TabIndex = 1;
            this._lastUpdateLabel.Text = "수신 대기 중...";
            //
            // _clearButton
            //
            this._clearButton.AutoSize = true;
            this._clearButton.Dock = System.Windows.Forms.DockStyle.Top;
            this._clearButton.Location = new System.Drawing.Point(9, 153);
            this._clearButton.Margin = new System.Windows.Forms.Padding(3, 4, 3, 4);
            this._clearButton.Name = "_clearButton";
            this._clearButton.Size = new System.Drawing.Size(75, 25);
            this._clearButton.TabIndex = 2;
            this._clearButton.Text = "지우기";
            this._clearButton.UseVisualStyleBackColor = true;
            this._clearButton.Click += new System.EventHandler(this.ClearButton_Click);
            //
            // _logLabel
            //
            this._logLabel.AutoSize = true;
            this._logLabel.Dock = System.Windows.Forms.DockStyle.Top;
            this._logLabel.Location = new System.Drawing.Point(9, 182);
            this._logLabel.Name = "_logLabel";
            this._logLabel.Padding = new System.Windows.Forms.Padding(0, 4, 0, 2);
            this._logLabel.Size = new System.Drawing.Size(52, 21);
            this._logLabel.TabIndex = 3;
            this._logLabel.Text = "수신 이력";
            //
            // _logBox
            //
            this._logBox.Dock = System.Windows.Forms.DockStyle.Fill;
            this._logBox.Font = new System.Drawing.Font(System.Drawing.FontFamily.GenericMonospace, 8.5F);
            this._logBox.Location = new System.Drawing.Point(9, 206);
            this._logBox.Multiline = true;
            this._logBox.Name = "_logBox";
            this._logBox.ReadOnly = true;
            this._logBox.ScrollBars = System.Windows.Forms.ScrollBars.Vertical;
            this._logBox.Size = new System.Drawing.Size(282, 305);
            this._logBox.TabIndex = 4;
            //
            // EspStatusPanel
            //
            this.Controls.Add(this._root);
            this.Name = "EspStatusPanel";
            this.Size = new System.Drawing.Size(300, 520);
            this._root.ResumeLayout(false);
            this._root.PerformLayout();
            this._channelGroup.ResumeLayout(false);
            this._channelGroup.PerformLayout();
            this._currentGroup.ResumeLayout(false);
            this._currentGroup.PerformLayout();
            this.ResumeLayout(false);
        }

        #endregion
    }
}
