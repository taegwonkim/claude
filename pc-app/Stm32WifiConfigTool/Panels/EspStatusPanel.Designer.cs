namespace Stm32WifiConfigTool.Panels
{
    partial class EspStatusPanel
    {
        /// <summary>Required designer variable.</summary>
        private System.ComponentModel.IContainer components = null;

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
            this._channelGroup.SuspendLayout();
            this._currentGroup.SuspendLayout();
            this.SuspendLayout();
            //
            // 아래 컨트롤은 전부 Dock/AutoSize/TableLayoutPanel을 쓰지 않고 각각 Location+Size를
            // 직접 갖는 자유 배치이므로, Visual Studio 디자이너에서 하나씩 선택해 크기 조절
            // 핸들을 드래그해 폭/높이를 자유롭게 바꿀 수 있다.
            //
            // _channelGroup
            //
            this._channelGroup.Controls.Add(this._showUsb);
            this._channelGroup.Controls.Add(this._showUart);
            this._channelGroup.Controls.Add(this._showBoth);
            this._channelGroup.Location = new System.Drawing.Point(9, 9);
            this._channelGroup.Name = "_channelGroup";
            this._channelGroup.Size = new System.Drawing.Size(282, 50);
            this._channelGroup.TabIndex = 0;
            this._channelGroup.TabStop = false;
            this._channelGroup.Text = "표시 채널";
            //
            // _showUsb
            //
            this._showUsb.Location = new System.Drawing.Point(10, 20);
            this._showUsb.Name = "_showUsb";
            this._showUsb.Size = new System.Drawing.Size(55, 22);
            this._showUsb.TabIndex = 0;
            this._showUsb.Text = "USB";
            this._showUsb.CheckedChanged += new System.EventHandler(this.ShowUsb_CheckedChanged);
            //
            // _showUart
            //
            this._showUart.Location = new System.Drawing.Point(70, 20);
            this._showUart.Name = "_showUart";
            this._showUart.Size = new System.Drawing.Size(60, 22);
            this._showUart.TabIndex = 1;
            this._showUart.Text = "UART";
            this._showUart.CheckedChanged += new System.EventHandler(this.ShowUart_CheckedChanged);
            //
            // _showBoth
            //
            this._showBoth.Checked = true;
            this._showBoth.Location = new System.Drawing.Point(140, 20);
            this._showBoth.Name = "_showBoth";
            this._showBoth.Size = new System.Drawing.Size(60, 22);
            this._showBoth.TabIndex = 2;
            this._showBoth.TabStop = true;
            this._showBoth.Text = "둘 다";
            this._showBoth.CheckedChanged += new System.EventHandler(this.ShowBoth_CheckedChanged);
            //
            // _currentGroup
            //
            this._currentGroup.Controls.Add(this._currentStatusLabel);
            this._currentGroup.Controls.Add(this._lastUpdateLabel);
            this._currentGroup.Location = new System.Drawing.Point(9, 68);
            this._currentGroup.Name = "_currentGroup";
            this._currentGroup.Size = new System.Drawing.Size(282, 90);
            this._currentGroup.TabIndex = 1;
            this._currentGroup.TabStop = false;
            this._currentGroup.Text = "현재 ESP32 상태";
            //
            // _currentStatusLabel
            //
            this._currentStatusLabel.Font = new System.Drawing.Font("Segoe UI", 16F, System.Drawing.FontStyle.Bold);
            this._currentStatusLabel.ForeColor = System.Drawing.Color.Gray;
            this._currentStatusLabel.Location = new System.Drawing.Point(10, 20);
            this._currentStatusLabel.Name = "_currentStatusLabel";
            this._currentStatusLabel.Size = new System.Drawing.Size(200, 30);
            this._currentStatusLabel.TabIndex = 0;
            this._currentStatusLabel.Text = "-";
            //
            // _lastUpdateLabel
            //
            this._lastUpdateLabel.ForeColor = System.Drawing.Color.DimGray;
            this._lastUpdateLabel.Location = new System.Drawing.Point(10, 58);
            this._lastUpdateLabel.Name = "_lastUpdateLabel";
            this._lastUpdateLabel.Size = new System.Drawing.Size(260, 20);
            this._lastUpdateLabel.TabIndex = 1;
            this._lastUpdateLabel.Text = "수신 대기 중...";
            //
            // _clearButton
            //
            this._clearButton.Location = new System.Drawing.Point(9, 166);
            this._clearButton.Name = "_clearButton";
            this._clearButton.Size = new System.Drawing.Size(80, 25);
            this._clearButton.TabIndex = 2;
            this._clearButton.Text = "지우기";
            this._clearButton.UseVisualStyleBackColor = true;
            this._clearButton.Click += new System.EventHandler(this.ClearButton_Click);
            //
            // _logLabel
            //
            this._logLabel.Location = new System.Drawing.Point(9, 198);
            this._logLabel.Name = "_logLabel";
            this._logLabel.Size = new System.Drawing.Size(160, 21);
            this._logLabel.TabIndex = 3;
            this._logLabel.Text = "수신 이력";
            //
            // _logBox
            //
            this._logBox.Font = new System.Drawing.Font(System.Drawing.FontFamily.GenericMonospace, 8.5F);
            this._logBox.Location = new System.Drawing.Point(9, 222);
            this._logBox.Multiline = true;
            this._logBox.Name = "_logBox";
            this._logBox.ReadOnly = true;
            this._logBox.ScrollBars = System.Windows.Forms.ScrollBars.Vertical;
            this._logBox.Size = new System.Drawing.Size(282, 289);
            this._logBox.TabIndex = 4;
            //
            // EspStatusPanel
            //
            this.Controls.Add(this._channelGroup);
            this.Controls.Add(this._currentGroup);
            this.Controls.Add(this._clearButton);
            this.Controls.Add(this._logLabel);
            this.Controls.Add(this._logBox);
            this.Name = "EspStatusPanel";
            this.Size = new System.Drawing.Size(300, 520);
            this._channelGroup.ResumeLayout(false);
            this._currentGroup.ResumeLayout(false);
            this.ResumeLayout(false);
        }

        #endregion
    }
}
